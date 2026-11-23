#pragma once
#include <QObject>
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

    // Called when the app quits: a running session is ended and saved rather than lost.
    void shutdown();

    focus::Database& database() { return db_; }
    void advanceClockForTesting(int seconds);  // shifts "now" so a test can skip minutes in milliseconds
    void tickNow() { onTick(); }

signals:
    void changed();
    void statsChanged();
    void tasksChanged();
    void appearanceChanged();
    void notification(const QString& title, const QString& body);
    void errorOccurred(const QString& message);

private:
    using Clock = focus::SessionController::Clock;
    Clock::time_point now() const { return Clock::now() + offset_; }
    void onTick();
    void refreshSnapshot();

    focus::Database db_;
    std::unique_ptr<focus::ActivitySource> source_;
    std::unique_ptr<focus::SessionController> controller_;
    focus::ControllerSnapshot snap_;
    QTimer timer_;
    std::chrono::seconds offset_{0};

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
