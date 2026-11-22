#include "controller/SessionController.h"

#include <algorithm>

#include "services/Reminders.h"
#include "storage/CheckInRepository.h"
#include "storage/SessionRepository.h"

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
      session_([this](const SessionEvent& e) { onSessionEvent(e); }) {
    if (config_.remindersEnabled) {
        for (auto& reminder : defaultReminders()) scheduler_.add(std::move(reminder));
    }
}

bool SessionController::start(SessionPlan plan, CheckIn before, Clock::time_point now) {
    if (session_.isActive()) return false;
    before.kind      = CheckInKind::BeforeSession;
    before.sessionId = std::nullopt;  // the session does not exist yet; it is linked when saved
    before.createdAt = unixNow();
    if (!before.isValid()) return false;

    eventNow_ = now;
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
