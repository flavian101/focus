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

    connect(&timer_, &QTimer::timeout, this, &FocusBackend::onTick);
    timer_.start(1000);
    refreshSnapshot();
    refreshStats();
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
                                int mood, int energy, const QString& tag) {
    focus::SessionPlan plan;
    plan.goal     = goal.trimmed().toStdString();
    plan.approach = approach.trimmed().toStdString();
    plan.duration = std::chrono::minutes(std::clamp(minutes, 1, 240));
    if (aiBudget >= 0) plan.aiBudget = aiBudget;

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
}

void FocusBackend::skipCheckIn() {
    controller_->skipAfterCheckIn();
    refreshSnapshot();
    refreshStats();
}

void FocusBackend::shutdown() {
    if (snap_.phase == focus::Phase::Focusing || snap_.phase == focus::Phase::Paused) abandon();
}

void FocusBackend::advanceClockForTesting(int seconds) { offset_ += std::chrono::seconds(seconds); }

// ----------------------------------------------------------------------------- refreshing

void FocusBackend::onTick() {
    const auto before = snap_.phase;
    try {
        controller_->tick(now());
    } catch (const focus::StorageError& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
    refreshSnapshot();
    if (before != snap_.phase && snap_.phase == focus::Phase::CheckIn) refreshStats();  // a session just ended
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
