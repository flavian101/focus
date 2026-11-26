#include "controller/SessionController.h"

#include <algorithm>
#include <functional>

#include "services/Reminders.h"
#include "storage/ActivityRepository.h"
#include "storage/AppRuleRepository.h"
#include "storage/CheckInRepository.h"
#include "storage/SessionRepository.h"
#include "storage/SettingsRepository.h"
#include "storage/TaskRepository.h"
#include "storage/WellnessRepository.h"

namespace focus {

namespace {
std::int64_t unixNow() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
}  // namespace

SessionController::SessionController(Database& db, ActivitySource& source, Configuration config, NotifyFn notify)
    : db_(db),
      source_(source),
      config_(config),
      notify_(std::move(notify)),
      wellness_(config.water, config.meals),
      session_([this](const SessionEvent& e) { onSessionEvent(e); }) {
    if (config_.remindersEnabled) {
        for (auto& reminder : defaultReminders()) scheduler_.add(std::move(reminder));
    }
    wellness_.setWaterGoal(SettingsRepository(db_).getInt("water_goal", config_.water.goalGlasses));
    trackingEnabled_ = SettingsRepository(db_).getInt("tracking", 1) != 0;
    loadTrackingSettings();
}

// ------------------------------------------------------------------- tracking modes, profile, rules

void SessionController::loadTrackingSettings() {
    SettingsRepository settings(db_);
    policy_ = config_.tracking;
    if (const auto m = settings.get("track_work"))  policy_.duringWork = parseMode(*m).value_or(policy_.duringWork);
    if (const auto m = settings.get("track_other")) policy_.otherwise  = parseMode(*m).value_or(policy_.otherwise);
    policy_.schedule.days        = static_cast<unsigned>(settings.getInt("work_days", static_cast<int>(policy_.schedule.days)));
    policy_.schedule.startMinute = settings.getInt("work_start", policy_.schedule.startMinute);
    policy_.schedule.endMinute   = settings.getInt("work_end", policy_.schedule.endMinute);
    if (!valid(policy_)) policy_ = config_.tracking;  // a damaged setting must not switch tracking off by accident

    profile_.role    = settings.get("profile_role").value_or("");
    profile_.purpose = settings.get("profile_purpose").value_or("");

    appRules_ = AppRuleRepository(db_).all();
    classifier_.setOverrides(appRules_);
}

TrackingMode SessionController::currentTrackingMode(const LocalTime& local) const {
    return trackingEnabled_ ? policy_.modeAt(local) : TrackingMode::Off;
}

bool SessionController::setTrackingPolicy(const TrackingPolicy& policy) {
    if (!valid(policy)) return false;
    policy_ = policy;
    SettingsRepository s(db_);
    s.set("track_work", std::string(modeName(policy.duringWork)));
    s.set("track_other", std::string(modeName(policy.otherwise)));
    s.setInt("work_days", static_cast<int>(policy.schedule.days));
    s.setInt("work_start", policy.schedule.startMinute);
    s.setInt("work_end", policy.schedule.endMinute);
    return true;
}

void SessionController::setProfile(UserProfile profile) {
    profile_ = cleaned(std::move(profile));
    SettingsRepository s(db_);
    s.set("profile_role", profile_.role);
    s.set("profile_purpose", profile_.purpose);
}

void SessionController::setAppCategory(const std::string& appName, ActivityCategory category, const std::string& day) {
    const std::string app = normalizedProcessName(appName);
    if (app.empty() || category == ActivityCategory::Away) return;

    AppRuleRepository(db_).set(app, category, unixNow());
    appRules_[app] = category;
    classifier_.setOverrides(appRules_);

    // Re-label what was already recorded today. For a browser only the "no opinion" time moves,
    // so the AI chats and video feeds inside it keep their own labels.
    const std::optional<ActivityCategory> onlyFrom = isBrowserProcess(app) ? std::optional(ActivityCategory::Neutral) : std::nullopt;
    tracker_.recategorize(day, app, onlyFrom, category);
    ActivityRepository(db_).recategorize(day, app, onlyFrom, category);
}

void SessionController::clearAppCategory(const std::string& appName) {
    const std::string app = normalizedProcessName(appName);
    AppRuleRepository(db_).remove(app);
    appRules_.erase(app);
    classifier_.setOverrides(appRules_);  // from now on; history keeps the label it was recorded with
}

bool SessionController::start(SessionPlan plan, CheckIn before, Clock::time_point now) {
    if (session_.isActive()) return false;
    before.kind      = CheckInKind::BeforeSession;
    before.sessionId = std::nullopt;  // the session does not exist yet; it is linked when saved
    before.createdAt = unixNow();
    if (!before.isValid()) return false;

    eventNow_ = now;
    // A task id that does not exist must not reach the database later, where the foreign key
    // would reject the finished session and lose it. Check now and start unlinked instead.
    if (plan.taskId && !TaskRepository(db_).find(*plan.taskId)) plan.taskId.reset();
    if (!session_.start(std::move(plan), now)) return false;
    before_          = std::move(before);
    awaitingCheckIn_ = false;  // starting again simply drops a check-in the user never answered
    lastFinished_.reset();
    present_      = true;
    lastSampleAt_ = now;
    return true;
}

void SessionController::pause(Clock::time_point now) {
    eventNow_ = now;
    session_.pause(now);
}

void SessionController::resume(Clock::time_point now) {
    eventNow_ = now;
    session_.resume(now);
}
void SessionController::logAiUse(Clock::time_point now) { session_.logManualAiUse(now); }

void SessionController::abandon(Clock::time_point now) {
    eventNow_ = now;
    session_.abandon(now);
    persistIfFinished();
}

void SessionController::tick(Clock::time_point now) {
    if (!session_.isActive()) return;
    eventNow_ = now;

    if (session_.state() == SessionState::Focusing && now - lastSampleAt_ >= config_.activitySampleInterval) {
        lastSampleAt_ = now;
        sampleActivity(now);
    }
    session_.tick(now);  // may end the session
    persistIfFinished();

    // Reminders only make sense while you are working AND at your desk. One that comes due
    // while you are away stays due, and is shown as soon as you are back.
    if (session_.state() == SessionState::Focusing && present_) {
        for (const auto& reminder : scheduler_.collectDue(now)) notify(reminder.title, reminder.message);
    }
}

void SessionController::sampleActivity(Clock::time_point now) {
    const auto idle = source_.idleFor();
    present_        = idle < config_.awayThreshold;
    if (!present_) {
        session_.recordAway(idle, now);
        return;
    }
    const auto window = source_.foreground();
    // Only the category is kept. The window title is dropped right here.
    session_.recordActivity(window ? classifier_.classify(*window) : ActivityCategory::Neutral, now);
}

void SessionController::onSessionEvent(const SessionEvent& event) {
    using Type    = SessionEvent::Type;
    const auto& s = session_.current();
    switch (event.type) {
        case Type::Started:
            scheduler_.start(eventNow_);  // the injected time, never the real clock
            notify("Focus started", s.goal);
            break;
        case Type::Paused:
            scheduler_.pause(eventNow_);
            break;
        case Type::Resumed:
            scheduler_.resume(eventNow_);
            break;
        case Type::AiVisit: {
            std::string count = std::to_string(s.aiTotal());
            if (s.aiBudget) count += " of " + std::to_string(*s.aiBudget);
            notify("AI visit " + count, "Be specific: what is the exact question, and what did you already try?");
            break;
        }
        case Type::AiBudgetExceeded:
            notify("AI budget used up",
                   "Write down what you've tried. Is this a real blocker, or are you avoiding the hard part?");
            break;
        case Type::Completed:
            notify("Session complete", "Nice work. Take a short break.");
            [[fallthrough]];
        case Type::Abandoned:
            scheduler_.stop();
            finishedUnsaved_ = true;
            break;
    }
}

void SessionController::persistIfFinished() {
    if (!finishedUnsaved_) return;
    finishedUnsaved_ = false;

    SessionRepository sessions(db_);
    CheckInRepository checkIns(db_);
    Transaction tx(db_);  // the session and its "before" check-in are saved together, or not at all
    lastSessionId_ = sessions.insert(session_.current());
    CheckIn b      = before_;
    b.sessionId    = lastSessionId_;
    checkIns.insert(b);
    tx.commit();

    lastFinished_        = session_.current();
    lastFinished_->id    = lastSessionId_;
    awaitingCheckIn_     = true;
}

void SessionController::submitAfterCheckIn(CheckIn after) {
    if (!awaitingCheckIn_) return;
    after.kind      = CheckInKind::AfterSession;
    after.sessionId = lastSessionId_;
    after.createdAt = unixNow();
    if (after.isValid()) CheckInRepository(db_).insert(after);  // an invalid one is dropped, not fatal
    awaitingCheckIn_ = false;
}

void SessionController::skipAfterCheckIn() { awaitingCheckIn_ = false; }

// ------------------------------------------------------------------------------ all-day activity

void SessionController::updateInputCounting(bool wanted) {
    if (wanted == inputCounting_) return;
    inputCounting_ = wanted;
    source_.setInputCounting(wanted);
}

void SessionController::trackActivity(const LocalTime& local, Clock::time_point now) {
    const TrackingMode mode = currentTrackingMode(local);
    updateInputCounting(keepsInputCounts(mode));  // listen only while the mode allows it, and stop the moment it does not
    if (mode == TrackingMode::Off) {
        if (!tracker_.pending().empty()) flushActivity();  // keep what was recorded before the mode switched off
        tracker_.forget();                                // and do not credit the time tracking is off
        haveForegroundKey_ = false;
        return;
    }

    if (!ambientSampled_ || now - lastAmbient_ >= config_.ambientSampleInterval) {
        const bool hadPrevious = ambientSampled_;
        const auto sinceLast   = std::chrono::duration_cast<std::chrono::seconds>(now - lastAmbient_);
        ambientSampled_ = true;
        lastAmbient_    = now;
        const auto idle = source_.idleFor();
        // Drained on every sample, away or not, so a count never sits around and gets credited to the wrong moment.
        const InputCounts counts = inputCounting_ ? source_.takeInputCounts() : InputCounts{};

        if (idle >= config_.awayThreshold) {
            tracker_.recordAway(now, local, idle);
        } else {
            const auto window = source_.foreground();
            ActivityCategory category = ActivityCategory::Neutral;
            std::string app           = "unknown";
            if (window) {
                category = classifier_.classify(*window);
                if (!window->processName.empty()) app = normalizedProcessName(window->processName);  // never the title
            }

            // Engagement: numbers about HOW the computer was used, never about WHAT was on screen or typed.
            SampleSignals engagement;
            if (keepsEngagement(mode)) {
                // Input arrived since the previous sample if the idle clock restarted after it.
                engagement.input = hadPrevious && idle < std::max(sinceLast, std::chrono::seconds(1));
                if (window) {
                    // Only a hash, only in memory, only to notice that the window or tab changed. If Windows briefly
                    // reports no window (mid alt-tab) the last one is kept, so one switch is not counted as two.
                    const std::size_t key = std::hash<std::string>{}(window->processName + '\n' + window->title);
                    if (haveForegroundKey_ && key != foregroundKey_) engagement.switches = 1;
                    foregroundKey_     = key;
                    haveForegroundKey_ = true;
                }
                if (keepsInputCounts(mode)) {
                    engagement.scrollEvents = counts.scrolls;
                    engagement.keyEvents    = counts.keys;
                    engagement.clickEvents  = counts.clicks;
                }
            }
            // Private mode keeps the category (so "40 min of distraction" is still known) but not which app.
            tracker_.record(now, local, category, keepsAppName(mode) ? app : std::string(), engagement);
        }
    }

    if (now - lastFlush_ >= config_.activityFlushInterval) {
        lastFlush_ = now;
        // Hold buckets back a little longer than the away threshold: that window is how far back
        // recordAway() may need to reach to correct what it already credited.
        const int holdBackMinutes = static_cast<int>(config_.awayThreshold.count() / 60) + 1;
        ActivityRepository(db_).addSeconds(tracker_.takeFlushable(local, holdBackMinutes));
    }
}

void SessionController::flushActivity() { ActivityRepository(db_).addSeconds(tracker_.takeAll()); }

void SessionController::setTrackingEnabled(bool enabled) {
    if (enabled == trackingEnabled_) return;
    trackingEnabled_ = enabled;
    SettingsRepository(db_).setInt("tracking", enabled ? 1 : 0);
    if (!enabled) {
        updateInputCounting(false);  // stop listening right now, not at the next tick
        flushActivity();             // keep what was recorded so far
        tracker_.forget();           // and do not credit the time tracking was off
        haveForegroundKey_ = false;
    }
}

DaySummary SessionController::daySummary(const std::string& day) {
    auto buckets = ActivityRepository(db_).forDay(day);
    for (const auto& pending : tracker_.pending()) {  // not saved yet, but part of today
        if (pending.day != day) continue;
        const auto same = std::find_if(buckets.begin(), buckets.end(), [&](const ActivityBucket& b) {
            return b.minuteOfDay == pending.minuteOfDay && b.category == pending.category && b.app == pending.app;
        });
        if (same != buckets.end()) same->accumulate(pending);
        else buckets.push_back(pending);
    }
    return summarizeDay(buckets, 60);  // a long list is fine: the UI scrolls it
}

void SessionController::clearActivity(const std::string& day) {
    tracker_.discardDay(day);
    ActivityRepository(db_).clearDay(day);
}

// ---------------------------------------------------------------------------- water and meals

void SessionController::logWater(const LocalTime& local) {
    WellnessRepository(db_).addGlass(local.day, local.minuteOfDay, unixNow());
}

bool SessionController::undoWater(const LocalTime& local) { return WellnessRepository(db_).undoLastGlass(local.day); }

void SessionController::setMeal(const LocalTime& local, const std::string& mealId, std::optional<MealStatus> status) {
    WellnessRepository repo(db_);
    if (status) repo.setMeal(local.day, mealId, *status, unixNow());
    else repo.clearMeal(local.day, mealId);
}

void SessionController::setWaterGoal(int glasses) {
    wellness_.setWaterGoal(glasses);
    SettingsRepository(db_).setInt("water_goal", wellness_.waterRule().goalGlasses);  // store the clamped value
}

WellnessSnapshot SessionController::wellness(const LocalTime& local) {
    WellnessRepository repo(db_);
    const auto water = repo.waterFor(local.day);
    const auto eaten = repo.mealsFor(local.day);

    WellnessSnapshot snap;
    snap.glasses        = water.glasses;
    snap.goal           = wellness_.waterRule().goalGlasses;
    snap.expected       = expectedGlasses(wellness_.waterRule(), local.minuteOfDay);
    snap.dayStartMinute = wellness_.waterRule().dayStartMinute;
    snap.dayEndMinute   = wellness_.waterRule().dayEndMinute;
    for (const auto& meal : wellness_.meals()) {
        std::optional<MealStatus> logged;
        if (const auto it = eaten.find(meal.id); it != eaten.end()) logged = it->second;
        snap.meals.push_back({meal.id, meal.name, meal.startMinute, meal.endMinute, mealState(meal, logged, local.minuteOfDay)});
    }
    return snap;
}

void SessionController::tickWellness(const LocalTime& local, Clock::time_point now) {
    using namespace std::chrono_literals;
    if (!config_.wellnessEnabled) return;
    if (wellnessChecked_ && now - lastWellnessCheck_ < 30s) return;
    wellnessChecked_     = true;
    lastWellnessCheck_   = now;

    WellnessRepository repo(db_);
    const bool present = source_.idleFor() < config_.awayThreshold;
    for (const auto& r : wellness_.due(local, repo.waterFor(local.day), repo.mealsFor(local.day), present)) {
        notify(r.title, r.body);
    }
}

PlanRules SessionController::planRules() const {
    PlanRules rules;
    rules.maxMust         = config_.maxMustPerDay;
    rules.capacityMinutes = static_cast<int>(config_.dailyFocusCapacity.count());
    return rules;
}

TaskResult SessionController::addTask(const std::string& day, std::string title, Priority priority, int estimateMinutes) {
    TaskRepository repo(db_);
    Task t;
    t.title           = std::move(title);
    t.priority        = priority;
    t.estimateMinutes = estimateMinutes;
    t.day             = day;
    t.createdAt       = unixNow();

    const auto check = checkTask(repo.forDay(day), t, planRules());
    if (check != PlanCheck::Ok) return TaskResult{false, describe(check, planRules()), 0};
    return TaskResult{true, {}, repo.insert(t)};
}

TaskResult SessionController::setTaskPriority(std::int64_t id, Priority priority) {
    TaskRepository repo(db_);
    auto existing = repo.find(id);
    if (!existing) return TaskResult{false, "That task no longer exists.", id};

    Task changed   = *existing;
    changed.priority = priority;
    const auto check = checkTask(repo.forDay(existing->day), changed, planRules());
    if (check != PlanCheck::Ok) return TaskResult{false, describe(check, planRules()), id};
    repo.setPriority(id, priority);
    return TaskResult{true, {}, id};
}

void SessionController::setTaskStatus(std::int64_t id, TaskStatus status) {
    TaskRepository repo(db_);
    auto existing = repo.find(id);
    if (!existing) return;
    // Bringing a dropped must-do back must not break the limit; quietly make it a Should if it would.
    if (status != TaskStatus::Dropped && existing->status == TaskStatus::Dropped && existing->priority == Priority::Must) {
        Task revived = *existing;
        revived.status = status;
        if (checkTask(repo.forDay(existing->day), revived, planRules()) == PlanCheck::TooManyMust) {
            repo.setPriority(id, Priority::Should);
        }
    }
    repo.setStatus(id, status, unixNow());
}

int SessionController::unfinishedBefore(const std::string& day) {
    return static_cast<int>(TaskRepository(db_).unfinishedBefore(day).size());
}

int SessionController::carryOverUnfinished(const std::string& today) {
    TaskRepository repo(db_);
    const auto stale = repo.unfinishedBefore(today);
    if (stale.empty()) return 0;

    Transaction tx(db_);  // move them all, or none
    for (const auto& old : stale) {  // already ordered oldest day first, then most important first
        Task moved = old;
        moved.day  = today;
        if (checkTask(repo.forDay(today), moved, planRules()) == PlanCheck::TooManyMust) {
            repo.setPriority(old.id, Priority::Should);  // today's must-do slots are full
        }
        repo.moveToDay(old.id, today);
    }
    tx.commit();
    return static_cast<int>(stale.size());
}

std::vector<Task> SessionController::tasksFor(const std::string& day) { return TaskRepository(db_).forDay(day); }

std::optional<Task> SessionController::task(std::int64_t id) { return TaskRepository(db_).find(id); }

PlanSummary SessionController::planFor(const std::string& day) {
    return summarizePlan(TaskRepository(db_).forDay(day), planRules());
}

ControllerSnapshot SessionController::snapshot(Clock::time_point now) const {
    ControllerSnapshot snap;
    const auto& s = session_.current();

    if (session_.state() == SessionState::Focusing) snap.phase = Phase::Focusing;
    else if (session_.state() == SessionState::Paused) snap.phase = Phase::Paused;
    else snap.phase = awaitingCheckIn_ ? Phase::CheckIn : Phase::Idle;

    if (snap.phase != Phase::Idle) {
        snap.goal             = s.goal;
        snap.approach         = s.approach;
        snap.plannedSeconds   = s.plannedSeconds;
        snap.secondsByCategory = s.secondsByCategory;
        snap.aiUsed           = s.aiTotal();
        snap.aiBudget         = s.aiBudget;
    }
    if (session_.isActive()) {
        snap.remainingSeconds = session_.remaining(now).count();
        if (snap.plannedSeconds > 0) {
            snap.progress = std::clamp(1.0 - static_cast<double>(snap.remainingSeconds) / snap.plannedSeconds, 0.0, 1.0);
        }
    } else if (snap.phase == Phase::CheckIn && s.plannedSeconds > 0) {
        snap.progress = std::clamp(static_cast<double>(s.activeSeconds) / s.plannedSeconds, 0.0, 1.0);
    }
    snap.present      = present_;
    snap.lastFinished = lastFinished_;
    return snap;
}

StatsReport SessionController::stats(int days) {
    const std::int64_t since = unixNow() - static_cast<std::int64_t>(std::max(days, 1)) * 24 * 3600;
    return summarize(SessionRepository(db_).startedSince(since), CheckInRepository(db_).createdSince(since));
}

std::vector<SessionSummary> SessionController::recentSessions(int limit) {
    return SessionRepository(db_).recent(limit);
}

void SessionController::notify(std::string title, std::string body) const {
    if (notify_) notify_(Notification{std::move(title), std::move(body)});
}

}  // namespace focus
