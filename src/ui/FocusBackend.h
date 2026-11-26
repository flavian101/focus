#pragma once
#include <QObject>
#include <QDateTime>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <chrono>
#include <memory>
#include <string>

#include "controller/SessionController.h"
#include "core/ActivitySource.h"
#include "storage/Database.h"

// The only bridge between QML and C++. QML reads properties and calls the Q_INVOKABLE methods;
// everything that matters (sessions, AI counting, saving) happens in focus::SessionController.
class FocusBackend : public QObject {
    Q_OBJECT

    // --- live session, refreshed about once a second ---------------------------------------
    Q_PROPERTY(QString phase READ phase NOTIFY changed)                // idle | focusing | paused | checkin
    Q_PROPERTY(QString goal READ goal NOTIFY changed)
    Q_PROPERTY(QString approach READ approach NOTIFY changed)
    Q_PROPERTY(QString remainingText READ remainingText NOTIFY changed)  // "24:31"
    Q_PROPERTY(double progress READ progress NOTIFY changed)             // 0..1
    Q_PROPERTY(int aiUsed READ aiUsed NOTIFY changed)
    Q_PROPERTY(int aiBudget READ aiBudget NOTIFY changed)                // -1 = no limit
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)   // [{key,label,seconds}]
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)          // the session that just ended

    // --- history, refreshed when a session ends -------------------------------------------
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(QVariantList week READ week NOTIFY statsChanged)          // [{label,minutes,today}]
    Q_PROPERTY(QVariantList recent READ recent NOTIFY statsChanged)      // [{goal,when,minutes,outcome,aiVisits}]

    // --- the day plan -------------------------------------------------------------------
    Q_PROPERTY(QString today READ today NOTIFY tasksChanged)             // "YYYY-MM-DD", local
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged)        // today's tasks, most important first
    Q_PROPERTY(QVariantMap plan READ plan NOTIFY tasksChanged)           // totals for today's plan
    Q_PROPERTY(int carryOverCount READ carryOverCount NOTIFY tasksChanged)  // unfinished tasks from earlier days

    // --- water and meals ----------------------------------------------------------------
    Q_PROPERTY(QVariantMap wellness READ wellness NOTIFY wellnessChanged)  // glasses, goal, pace, meals[]

    // --- all-day activity ----------------------------------------------------------------
    Q_PROPERTY(QVariantMap activityDay READ activityDay NOTIFY activityChanged)   // totals, engagement, hours[], apps[]
    Q_PROPERTY(bool trackingEnabled READ trackingEnabled NOTIFY activityChanged)

    // --- tracking modes and the profile -------------------------------------------------
    Q_PROPERTY(QString trackingMode READ trackingMode NOTIFY trackingChanged)     // detailed | work | private | off, right now
    Q_PROPERTY(QVariantMap trackingPolicy READ trackingPolicy NOTIFY trackingChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY trackingChanged)           // role, purpose

    // --- appearance (saved between runs) ------------------------------------------------------
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY appearanceChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY appearanceChanged)

public:
    // persistSettings=false keeps tests from writing the user's settings file.
    FocusBackend(const std::string& dbPath, std::unique_ptr<focus::ActivitySource> source,
                 bool persistSettings = true, QObject* parent = nullptr);

    QString phase() const;
    QString goal() const;
    QString approach() const;
    QString remainingText() const;
    double progress() const;
    int aiUsed() const;
    int aiBudget() const;
    bool present() const;
    QVariantList categories() const;
    QVariantMap summary() const;
    QVariantMap stats() const { return stats_; }
    QVariantList week() const { return week_; }
    QVariantList recent() const { return recent_; }

    QString trackingMode() const { return trackingMode_; }
    QVariantMap trackingPolicy() const { return trackingPolicy_; }
    QVariantMap profile() const { return profile_; }
    QVariantMap activityDay() const { return activityDay_; }
    bool trackingEnabled() const { return controller_->trackingEnabled(); }
    QVariantMap wellness() const { return wellness_; }
    QString today() const { return today_; }
    QVariantList tasks() const { return tasks_; }
    QVariantMap plan() const { return plan_; }
    int carryOverCount() const { return carryOver_; }

    bool darkMode() const { return darkMode_; }
    QString accent() const { return accent_; }
    void setDarkMode(bool dark);
    void setAccent(const QString& color);

    // taskId 0 = not linked to a planned task
    Q_INVOKABLE bool startSession(const QString& goal, const QString& approach, int minutes, int aiBudget,
                                  int mood, int energy, const QString& tag, int taskId = 0);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void abandon();
    Q_INVOKABLE void logAiUse();
    Q_INVOKABLE void submitCheckIn(int mood, int energy, const QString& tag);
    Q_INVOKABLE void skipCheckIn();
    Q_INVOKABLE void refreshStats();

    // Plan commands. The ones that can be refused return the reason as text ("" = it worked).
    Q_INVOKABLE QString addTask(const QString& title, int priority, int minutes);
    Q_INVOKABLE QString cyclePriority(int taskId);
    Q_INVOKABLE void setTaskDone(int taskId, bool done);
    Q_INVOKABLE void dropTask(int taskId);
    Q_INVOKABLE void restoreTask(int taskId);
    Q_INVOKABLE int carryOver();
    Q_INVOKABLE void refreshTasks();

    // Water and meals. setMeal's status is "ate", "skipped" or "clear".
    Q_INVOKABLE void logWater();
    Q_INVOKABLE void undoWater();
    Q_INVOKABLE void setWaterGoal(int glasses);
    Q_INVOKABLE void setMeal(const QString& mealId, const QString& status);
    Q_INVOKABLE void refreshWellness();

    // modes are "work", "private" or "off"; daysMask has bit 0 = Monday ... bit 6 = Sunday. Returns false if it does not make sense.
    Q_INVOKABLE bool setTrackingPolicy(const QString& workMode, const QString& otherMode, int daysMask, int startMinute, int endMinute);
    Q_INVOKABLE void setProfile(const QString& role, const QString& purpose);
    // "This app is <category>": category is "focus", "neutral", "distraction" or "ai"; "" resets the correction.
    Q_INVOKABLE void setAppCategory(const QString& app, const QString& category);
    Q_INVOKABLE void refreshTracking();

    Q_INVOKABLE void setTrackingEnabled(bool enabled);
    Q_INVOKABLE void clearTodayActivity();
    Q_INVOKABLE void refreshActivity();

    // Called when the app quits: a running session is ended and saved rather than lost.
    void shutdown();

    focus::Database& database() { return db_; }
    void advanceClockForTesting(int seconds);  // shifts "now" so a test can skip minutes in milliseconds
    void tickNow() { onTick(); }
    void setLocalTimeForTesting(const QDateTime& when) { fixedLocal_ = when; }  // freezes "what time is it" for wellness

signals:
    void changed();
    void statsChanged();
    void tasksChanged();
    void wellnessChanged();
    void activityChanged();
    void trackingChanged();
    void appearanceChanged();
    void notification(const QString& title, const QString& body);
    void errorOccurred(const QString& message);

private:
    using Clock = focus::SessionController::Clock;
    Clock::time_point now() const { return Clock::now() + offset_; }
    void onTick();
    void refreshSnapshot();
    focus::LocalTime localTime() const;

    focus::Database db_;
    std::unique_ptr<focus::ActivitySource> source_;
    std::unique_ptr<focus::SessionController> controller_;
    focus::ControllerSnapshot snap_;
    QTimer timer_;
    std::chrono::seconds offset_{0};

    QDateTime fixedLocal_;  // only set by tests
    int tickCount_ = 0;
    QVariantMap wellness_;
    QVariantMap activityDay_;
    QString trackingMode_ = QStringLiteral("work");
    QVariantMap trackingPolicy_;
    QVariantMap profile_;

    QString today_;
    QVariantList tasks_;
    QVariantMap plan_;
    int carryOver_ = 0;

    QVariantMap stats_;
    QVariantList week_;
    QVariantList recent_;

    bool persistSettings_;
    bool darkMode_ = true;
    QString accent_ = QStringLiteral("#7C5CFF");
};
