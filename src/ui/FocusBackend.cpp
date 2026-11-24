#include "FocusBackend.h"

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QSettings>
#include <algorithm>

#include "storage/Migrations.h"

namespace {

QString formatClock(std::int64_t seconds) {
    seconds = std::max<std::int64_t>(seconds, 0);
    const auto h = seconds / 3600, m = (seconds / 60) % 60, s = seconds % 60;
    if (h > 0) return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

QString currentDay() { return QDate::currentDate().toString(Qt::ISODate); }

QString minuteClock(int minute) {
    return minute < 0 ? QString() : QString::fromStdString(focus::clockText(minute));
}

QString stateName(focus::MealState s) {
    switch (s) {
        case focus::MealState::Upcoming: return QStringLiteral("upcoming");
        case focus::MealState::Due:      return QStringLiteral("due");
        case focus::MealState::Ate:      return QStringLiteral("ate");
        case focus::MealState::Skipped:  return QStringLiteral("skipped");
        case focus::MealState::Missed:   return QStringLiteral("missed");
    }
    return QStringLiteral("upcoming");
}

QString whenText(qint64 unixSeconds) {
    const auto when  = QDateTime::fromSecsSinceEpoch(unixSeconds);
    const auto days  = when.date().daysTo(QDate::currentDate());
    const auto time  = QLocale().toString(when.time(), QStringLiteral("HH:mm"));
    if (days == 0) return QStringLiteral("Today %1").arg(time);
    if (days == 1) return QStringLiteral("Yesterday %1").arg(time);
    return QStringLiteral("%1 %2").arg(QLocale().toString(when.date(), QStringLiteral("ddd")), time);
}

}  // namespace

FocusBackend::FocusBackend(const std::string& dbPath, std::unique_ptr<focus::ActivitySource> source,
                           bool persistSettings, QObject* parent)
    : QObject(parent),
      db_(focus::Database::open(dbPath)),
      source_(std::move(source)),
      persistSettings_(persistSettings) {
    focus::migrate(db_);

    controller_ = std::make_unique<focus::SessionController>(
        db_, *source_, focus::Configuration::defaults(), [this](const focus::Notification& n) {
            emit notification(QString::fromStdString(n.title), QString::fromStdString(n.body));
        });

    if (persistSettings_) {
        QSettings settings;
        darkMode_ = settings.value(QStringLiteral("darkMode"), true).toBool();
        accent_   = settings.value(QStringLiteral("accent"), accent_).toString();
    }

    today_ = currentDay();

    connect(&timer_, &QTimer::timeout, this, &FocusBackend::onTick);
    timer_.start(1000);
    refreshSnapshot();
    refreshStats();
    refreshTasks();
    refreshWellness();
    refreshActivity();
}

// ------------------------------------------------------------------------------ properties

QString FocusBackend::phase() const {
    switch (snap_.phase) {
        case focus::Phase::Idle:     return QStringLiteral("idle");
        case focus::Phase::Focusing: return QStringLiteral("focusing");
        case focus::Phase::Paused:   return QStringLiteral("paused");
        case focus::Phase::CheckIn:  return QStringLiteral("checkin");
    }
    return QStringLiteral("idle");
}

QString FocusBackend::goal() const { return QString::fromStdString(snap_.goal); }
QString FocusBackend::approach() const { return QString::fromStdString(snap_.approach); }
QString FocusBackend::remainingText() const { return formatClock(snap_.remainingSeconds); }
double FocusBackend::progress() const { return snap_.progress; }
int FocusBackend::aiUsed() const { return snap_.aiUsed; }
int FocusBackend::aiBudget() const { return snap_.aiBudget.value_or(-1); }
bool FocusBackend::present() const { return snap_.present; }

QVariantList FocusBackend::categories() const {
    using focus::ActivityCategory;
    struct Row { ActivityCategory c; const char* label; };
    static const Row rows[] = {{ActivityCategory::Focus, "Focus"},
                               {ActivityCategory::AiAssistant, "AI"},
                               {ActivityCategory::Neutral, "Other"},
                               {ActivityCategory::Distraction, "Distraction"},
                               {ActivityCategory::Away, "Away"}};
    QVariantList out;
    for (const auto& r : rows) {
        QVariantMap m;
        m[QStringLiteral("key")]     = QString::fromUtf8(focus::categoryName(r.c).data(), static_cast<int>(focus::categoryName(r.c).size()));
        m[QStringLiteral("label")]   = QString::fromUtf8(r.label);
        m[QStringLiteral("seconds")] = static_cast<qlonglong>(snap_.secondsByCategory[static_cast<std::size_t>(r.c)]);
        out.push_back(m);
    }
    return out;
}

QVariantMap FocusBackend::summary() const {
    QVariantMap m;
    if (!snap_.lastFinished) return m;
    const auto& s = *snap_.lastFinished;
    m[QStringLiteral("goal")]           = QString::fromStdString(s.goal);
    m[QStringLiteral("completed")]      = s.outcome == focus::SessionOutcome::Completed;
    m[QStringLiteral("activeMinutes")]  = static_cast<int>(s.activeSeconds / 60);
    m[QStringLiteral("plannedMinutes")] = static_cast<int>(s.plannedSeconds / 60);
    m[QStringLiteral("aiVisits")]       = s.aiTotal();
    m[QStringLiteral("aiBudget")]       = s.aiBudget.value_or(-1);
    m[QStringLiteral("awayMinutes")]    = static_cast<int>(s.secondsIn(focus::ActivityCategory::Away) / 60);
    m[QStringLiteral("taskId")]         = 0;
    m[QStringLiteral("taskTitle")]      = QString();
    m[QStringLiteral("taskDone")]       = false;
    if (s.taskId) {
        if (const auto t = controller_->task(*s.taskId)) {
            m[QStringLiteral("taskId")]    = static_cast<int>(t->id);
            m[QStringLiteral("taskTitle")] = QString::fromStdString(t->title);
            m[QStringLiteral("taskDone")]  = t->status == focus::TaskStatus::Done;
        }
    }
    return m;
}

void FocusBackend::setDarkMode(bool dark) {
    if (darkMode_ == dark) return;
    darkMode_ = dark;
    if (persistSettings_) QSettings().setValue(QStringLiteral("darkMode"), dark);
    emit appearanceChanged();
}

void FocusBackend::setAccent(const QString& color) {
    if (accent_ == color) return;
    accent_ = color;
    if (persistSettings_) QSettings().setValue(QStringLiteral("accent"), color);
    emit appearanceChanged();
}

// ------------------------------------------------------------------------------- commands

bool FocusBackend::startSession(const QString& goal, const QString& approach, int minutes, int aiBudget,
                                int mood, int energy, const QString& tag, int taskId) {
    focus::SessionPlan plan;
    plan.goal     = goal.trimmed().toStdString();
    plan.approach = approach.trimmed().toStdString();
    plan.duration = std::chrono::minutes(std::clamp(minutes, 1, 240));
    if (aiBudget >= 0) plan.aiBudget = aiBudget;
    if (taskId > 0) plan.taskId = taskId;

    focus::CheckIn before;
    before.mood   = mood;
    before.energy = energy;
    before.tag    = tag.trimmed().left(32).toStdString();

    bool ok = false;
    try {
        ok = controller_->start(plan, before, now());
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshSnapshot();
    return ok;
}

void FocusBackend::pause() { controller_->pause(now()); refreshSnapshot(); }
void FocusBackend::resume() { controller_->resume(now()); refreshSnapshot(); }
void FocusBackend::logAiUse() { controller_->logAiUse(now()); refreshSnapshot(); }

void FocusBackend::abandon() {
    try {
        controller_->abandon(now());
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshSnapshot();
    refreshStats();
    refreshTasks();
}

void FocusBackend::submitCheckIn(int mood, int energy, const QString& tag) {
    focus::CheckIn after;
    after.mood   = mood;
    after.energy = energy;
    after.tag    = tag.trimmed().left(32).toStdString();
    try {
        controller_->submitAfterCheckIn(after);
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshSnapshot();
    refreshStats();
    refreshTasks();
}

void FocusBackend::skipCheckIn() {
    controller_->skipAfterCheckIn();
    refreshSnapshot();
    refreshStats();
    refreshTasks();
}

// ------------------------------------------------------------------------------- day plan

QString FocusBackend::addTask(const QString& title, int priority, int minutes) {
    const auto p = static_cast<focus::Priority>(std::clamp(priority, 1, 3));
    QString message;
    try {
        const auto result = controller_->addTask(today_.toStdString(), title.trimmed().toStdString(), p, minutes);
        if (!result.ok) message = QString::fromStdString(result.message);
    } catch (const focus::StorageError& e) {
        message = QString::fromUtf8(e.what());
    }
    refreshTasks();
    return message;
}

// Must -> Should -> Could -> Must. If the next step is refused (the must-do limit), try the one after it.
QString FocusBackend::cyclePriority(int taskId) {
    int current = 0;
    for (const auto& v : tasks_) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("taskId")).toInt() == taskId) current = m.value(QStringLiteral("priority")).toInt();
    }
    if (current == 0) return {};

    QString message;
    for (int step = 1; step <= 2; ++step) {
        const int next = (current - 1 + step) % 3 + 1;
        const auto result = controller_->setTaskPriority(taskId, static_cast<focus::Priority>(next));
        if (result.ok) break;
        message = QString::fromStdString(result.message);  // explain why a step was skipped
    }
    refreshTasks();
    return message;
}

void FocusBackend::setTaskDone(int taskId, bool done) {
    controller_->setTaskStatus(taskId, done ? focus::TaskStatus::Done : focus::TaskStatus::Todo);
    refreshTasks();
}

void FocusBackend::dropTask(int taskId) {
    controller_->setTaskStatus(taskId, focus::TaskStatus::Dropped);
    refreshTasks();
}

void FocusBackend::restoreTask(int taskId) {
    controller_->setTaskStatus(taskId, focus::TaskStatus::Todo);
    refreshTasks();
}

int FocusBackend::carryOver() {
    int moved = 0;
    try {
        moved = controller_->carryOverUnfinished(today_.toStdString());
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshTasks();
    return moved;
}

void FocusBackend::refreshTasks() {
    const std::string day = today_.toStdString();
    tasks_.clear();
    for (const auto& t : controller_->tasksFor(day)) {
        QVariantMap m;
        m[QStringLiteral("taskId")]        = static_cast<int>(t.id);
        m[QStringLiteral("title")]         = QString::fromStdString(t.title);
        m[QStringLiteral("priority")]      = static_cast<int>(t.priority);
        m[QStringLiteral("priorityLabel")] = QString::fromUtf8(focus::priorityLabel(t.priority).data(),
                                                               static_cast<int>(focus::priorityLabel(t.priority).size()));
        m[QStringLiteral("estimate")]      = t.estimateMinutes;
        m[QStringLiteral("actualMinutes")] = static_cast<int>(t.actualSeconds / 60);
        m[QStringLiteral("done")]          = t.status == focus::TaskStatus::Done;
        m[QStringLiteral("dropped")]       = t.status == focus::TaskStatus::Dropped;
        tasks_.push_back(m);
    }

    const auto s = controller_->planFor(day);
    QVariantMap p;
    p[QStringLiteral("total")]           = s.total;
    p[QStringLiteral("done")]            = s.done;
    p[QStringLiteral("dropped")]         = s.dropped;
    p[QStringLiteral("mustTotal")]       = s.mustTotal;
    p[QStringLiteral("mustDone")]        = s.mustDone;
    p[QStringLiteral("maxMust")]         = controller_->config().maxMustPerDay;
    p[QStringLiteral("plannedMinutes")]  = s.plannedMinutes;
    p[QStringLiteral("doneMinutes")]     = s.doneMinutes;
    p[QStringLiteral("actualMinutes")]   = s.actualMinutes;
    p[QStringLiteral("capacityMinutes")] = s.capacityMinutes;
    p[QStringLiteral("overcommitted")]   = s.overcommitted;
    p[QStringLiteral("completion")]      = s.completion;
    plan_ = p;

    carryOver_ = controller_->unfinishedBefore(day);
    emit tasksChanged();
}

void FocusBackend::shutdown() {
    if (snap_.phase == focus::Phase::Focusing || snap_.phase == focus::Phase::Paused) abandon();
    try {
        controller_->flushActivity();  // minutes still held in memory must reach the database
    } catch (const focus::StorageError& e) {
        qWarning("Could not save activity on exit: %s", e.what());
    }
}

void FocusBackend::advanceClockForTesting(int seconds) { offset_ += std::chrono::seconds(seconds); }

// ----------------------------------------------------------------------------- refreshing

focus::LocalTime FocusBackend::localTime() const {
    const QDateTime t = fixedLocal_.isValid() ? fixedLocal_ : QDateTime::currentDateTime();
    return focus::LocalTime{t.date().toString(Qt::ISODate).toStdString(), t.time().hour() * 60 + t.time().minute()};
}

// ------------------------------------------------------------------------------ all-day activity

void FocusBackend::setTrackingEnabled(bool enabled) {
    controller_->setTrackingEnabled(enabled);
    refreshActivity();
}

void FocusBackend::clearTodayActivity() {
    controller_->clearActivity(today_.toStdString());
    refreshActivity();
}

void FocusBackend::refreshActivity() {
    using focus::ActivityCategory;
    const auto d = controller_->daySummary(today_.toStdString());
    auto minutes = [&](ActivityCategory c) { return static_cast<double>(d.secondsByCategory[static_cast<std::size_t>(c)]) / 60.0; };

    QVariantMap m;
    m[QStringLiteral("activeMinutes")]      = d.activeSeconds / 60.0;
    m[QStringLiteral("focusMinutes")]       = minutes(ActivityCategory::Focus);
    m[QStringLiteral("aiMinutes")]          = minutes(ActivityCategory::AiAssistant);
    m[QStringLiteral("distractionMinutes")] = minutes(ActivityCategory::Distraction);
    m[QStringLiteral("awayMinutes")]        = minutes(ActivityCategory::Away);
    m[QStringLiteral("longestFocus")]       = d.longestFocusMinutes;
    m[QStringLiteral("switches")]           = d.contextSwitches;
    m[QStringLiteral("switchesPerHour")]    = d.switchesPerActiveHour;
    m[QStringLiteral("firstText")]          = minuteClock(d.firstActiveMinute);
    m[QStringLiteral("lastText")]           = minuteClock(d.lastActiveMinute);

    QVariantList hours;
    for (const auto& h : d.hours) {
        QVariantMap hm;
        auto at = [&](ActivityCategory c) { return h.seconds[static_cast<std::size_t>(c)] / 60.0; };
        hm[QStringLiteral("hour")]        = h.hour;
        hm[QStringLiteral("focus")]       = at(ActivityCategory::Focus);
        hm[QStringLiteral("ai")]          = at(ActivityCategory::AiAssistant);
        hm[QStringLiteral("neutral")]     = at(ActivityCategory::Neutral);
        hm[QStringLiteral("distraction")] = at(ActivityCategory::Distraction);
        hm[QStringLiteral("away")]        = at(ActivityCategory::Away);
        hours.push_back(hm);
    }
    m[QStringLiteral("hours")] = hours;

    QVariantList apps;
    const double top = d.topApps.empty() ? 1.0 : static_cast<double>(d.topApps.front().seconds);
    for (const auto& a : d.topApps) {
        if (a.seconds < 30) continue;  // under half a minute is noise, not a row in a list
        QVariantMap am;
        am[QStringLiteral("app")]      = QString::fromStdString(a.app);
        am[QStringLiteral("category")] = QString::fromUtf8(focus::categoryName(a.category).data(), static_cast<int>(focus::categoryName(a.category).size()));
        am[QStringLiteral("minutes")]  = a.seconds / 60.0;
        am[QStringLiteral("share")]    = a.seconds / top;  // 1.0 for the most used app: for drawing proportional bars
        apps.push_back(am);
    }
    m[QStringLiteral("apps")] = apps;
    activityDay_ = m;
    emit activityChanged();
}

// -------------------------------------------------------------------------- water and meals

void FocusBackend::logWater() {
    controller_->logWater(localTime());
    refreshWellness();
}

void FocusBackend::undoWater() {
    controller_->undoWater(localTime());
    refreshWellness();
}

void FocusBackend::setWaterGoal(int glasses) {
    controller_->setWaterGoal(glasses);
    refreshWellness();
}

void FocusBackend::setMeal(const QString& mealId, const QString& status) {
    std::optional<focus::MealStatus> s;
    if (status == QLatin1String("ate")) s = focus::MealStatus::Ate;
    else if (status == QLatin1String("skipped")) s = focus::MealStatus::Skipped;
    controller_->setMeal(localTime(), mealId.toStdString(), s);  // anything else clears the answer
    refreshWellness();
}

void FocusBackend::refreshWellness() {
    const auto w = controller_->wellness(localTime());
    QVariantMap m;
    m[QStringLiteral("glasses")]   = w.glasses;
    m[QStringLiteral("goal")]      = w.goal;
    m[QStringLiteral("expected")]  = w.expected;
    m[QStringLiteral("behind")]    = std::max(0, w.expected - w.glasses);
    m[QStringLiteral("onPace")]    = w.glasses >= w.expected;
    m[QStringLiteral("startText")] = QString::fromStdString(focus::clockText(w.dayStartMinute));
    m[QStringLiteral("endText")]   = QString::fromStdString(focus::clockText(w.dayEndMinute));
    QVariantList meals;
    for (const auto& meal : w.meals) {
        QVariantMap mm;
        mm[QStringLiteral("id")]     = QString::fromStdString(meal.id);
        mm[QStringLiteral("name")]   = QString::fromStdString(meal.name);
        mm[QStringLiteral("window")] = QStringLiteral("%1 \u2013 %2").arg(QString::fromStdString(focus::clockText(meal.startMinute)),
                                                                          QString::fromStdString(focus::clockText(meal.endMinute)));
        mm[QStringLiteral("state")]  = stateName(meal.state);
        meals.push_back(mm);
    }
    m[QStringLiteral("meals")] = meals;
    wellness_ = m;
    emit wellnessChanged();
}

void FocusBackend::onTick() {
    if (const QString day = currentDay(); day != today_) {  // the app was left running past midnight
        today_ = day;
        refreshTasks();
        refreshStats();
    }
    try {
        controller_->trackActivity(localTime(), now());  // all-day activity, session or not
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    if (tickCount_ % 30 == 0) refreshActivity();
    controller_->tickWellness(localTime(), now());  // water and meals run all day, session or not
    if (++tickCount_ % 15 == 0) refreshWellness();  // pace and meal states drift with the clock

    const auto before = snap_.phase;
    try {
        controller_->tick(now());
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshSnapshot();
    if (before != snap_.phase && snap_.phase == focus::Phase::CheckIn) {  // a session just ended
        refreshStats();
        refreshTasks();  // time spent on the linked task changed
    }
}

void FocusBackend::refreshSnapshot() {
    snap_ = controller_->snapshot(now());
    emit changed();
}

void FocusBackend::refreshStats() {
    const auto r = controller_->stats(7);
    QVariantMap m;
    m[QStringLiteral("completed")]    = r.sessionsCompleted;
    m[QStringLiteral("abandoned")]    = r.sessionsAbandoned;
    m[QStringLiteral("focusMinutes")] = static_cast<double>(r.focusSeconds) / 60.0;
    m[QStringLiteral("awayMinutes")]  = static_cast<double>(r.awaySeconds) / 60.0;
    m[QStringLiteral("aiPerSession")] = r.aiVisitsPerSession;
    m[QStringLiteral("aiSharePct")]   = r.aiTimeShare * 100.0;
    m[QStringLiteral("overBudget")]   = r.sessionsOverAiBudget;
    m[QStringLiteral("hasMood")]      = r.avgMoodBefore.has_value() && r.avgMoodAfter.has_value();
    m[QStringLiteral("moodBefore")]   = r.avgMoodBefore.value_or(0.0);
    m[QStringLiteral("moodAfter")]    = r.avgMoodAfter.value_or(0.0);
    m[QStringLiteral("moodChange")]   = r.avgMoodChange.value_or(0.0);
    QStringList tags;
    for (const auto& [tag, count] : r.topTags) tags << QStringLiteral("%1 (%2)").arg(QString::fromStdString(tag)).arg(count);
    m[QStringLiteral("tags")] = tags.join(QStringLiteral(", "));
    stats_ = m;

    // Focus minutes per local calendar day for the last 7 days, oldest first.
    const auto sessions = controller_->recentSessions(500);
    const QDate today   = QDate::currentDate();
    week_.clear();
    for (int back = 6; back >= 0; --back) {
        const QDate day = today.addDays(-back);
        double minutes  = 0;
        for (const auto& s : sessions) {
            if (QDateTime::fromSecsSinceEpoch(s.startedAt).date() == day) minutes += static_cast<double>(s.activeSeconds) / 60.0;
        }
        QVariantMap d;
        d[QStringLiteral("label")]   = QLocale().toString(day, QStringLiteral("ddd"));
        d[QStringLiteral("minutes")] = minutes;
        d[QStringLiteral("today")]   = back == 0;
        week_.push_back(d);
    }

    recent_.clear();
    for (std::size_t i = 0; i < sessions.size() && i < 6; ++i) {
        const auto& s = sessions[i];
        QVariantMap d;
        d[QStringLiteral("goal")]      = QString::fromStdString(s.goal);
        d[QStringLiteral("when")]      = whenText(s.startedAt);
        d[QStringLiteral("minutes")]   = static_cast<int>(s.activeSeconds / 60);
        d[QStringLiteral("completed")] = s.outcome == focus::SessionOutcome::Completed;
        d[QStringLiteral("aiVisits")]  = s.aiTotal();
        d[QStringLiteral("overBudget")] = s.aiBudget.has_value() && s.aiTotal() > *s.aiBudget;
        recent_.push_back(d);
    }
    emit statsChanged();
}
