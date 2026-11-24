// Headless self-test: loads the real QML, walks through a whole session, checks the QML
// produced no warnings, and saves screenshots. Run via `ctest` or `focus_ui --self-test <dir>`.
#include <QApplication>
#include <QDate>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <ctime>

#include "FocusBackend.h"
#include "UiApp.h"
#include "core/ActivitySource.h"
#include "storage/ActivityRepository.h"
#include "storage/CheckInRepository.h"
#include "storage/SessionRepository.h"
#include "storage/TaskRepository.h"

namespace {

int g_qmlWarnings = 0;
QtMessageHandler g_previousHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    // Anything QML complains about (missing property, binding loop, bad import...) counts as a failure.
    if (type != QtDebugMsg && type != QtInfoMsg && (msg.contains(QLatin1String(".qml")) || msg.contains(QLatin1String("QML")))) {
        ++g_qmlWarnings;
    }
    if (g_previousHandler) g_previousHandler(type, ctx, msg);
}

void wait(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int g_failures = 0;
void check(bool ok, const char* what) {
    std::printf("[%s] %s\n", ok ? " ok " : "FAIL", what);
    if (!ok) ++g_failures;
}

void seedHistory(focus::Database& db) {
    focus::SessionRepository sessions(db);
    focus::CheckInRepository checkIns(db);
    const std::int64_t now = std::time(nullptr);

    struct Row { int daysAgo; int hour; const char* goal; int active; bool done; int ai; int budget; int focusPct; int mood0, mood1; const char* tag; };
    const Row rows[] = {
        {6, 9,  "Refactor the auth module",     25, true,  1, 3, 80, 3, 4, "calm"},
        {5, 14, "Write unit tests for billing", 25, true,  2, 3, 70, 2, 4, "stuck"},
        {5, 16, "Fix the flaky CI job",         12, false, 4, 2, 40, 2, 2, "anxious"},
        {4, 10, "Read the Spring Security docs", 25, true, 0, 3, 90, 3, 5, "curious"},
        {3, 11, "Angular form validation",      25, true,  3, 3, 60, 3, 4, "stuck"},
        {2, 15, "Design the dashboard layout",  50, true,  1, 4, 85, 4, 5, "calm"},
        {1, 9,  "Implement CheckIn storage",    25, true,  2, 3, 75, 3, 4, "focused"},
        {1, 13, "Debug the null pointer crash", 18, false, 5, 2, 35, 2, 3, "stuck"},
        {0, 8,  "Plan the week",                25, true,  0, 3, 92, 4, 5, "calm"},
    };
    for (const auto& r : rows) {
        focus::SessionSummary s;
        s.goal           = r.goal;
        s.approach       = "Read the code and try it myself first";
        s.plannedSeconds = r.active * 60 + (r.done ? 0 : 600);
        s.activeSeconds  = r.active * 60;
        s.startedAt      = now - r.daysAgo * 86400 - r.hour * 600;
        s.endedAt        = s.startedAt + s.activeSeconds;
        s.outcome        = r.done ? focus::SessionOutcome::Completed : focus::SessionOutcome::Abandoned;
        s.aiBudget       = r.budget;
        s.aiVisits       = r.ai;
        const std::int64_t total = s.activeSeconds;
        using C = focus::ActivityCategory;
        auto at = [&](C c) -> std::int64_t& { return s.secondsByCategory[static_cast<std::size_t>(c)]; };
        at(C::Focus)       = total * r.focusPct / 100;
        at(C::AiAssistant) = total * std::min(r.ai * 6, 30) / 100;
        at(C::Neutral)     = total - at(C::Focus) - at(C::AiAssistant);
        const auto id = sessions.insert(s);

        focus::CheckIn before, after;
        before.sessionId = after.sessionId = id;
        before.kind = focus::CheckInKind::BeforeSession;
        after.kind  = focus::CheckInKind::AfterSession;
        before.mood = r.mood0;
        after.mood  = r.mood1;
        before.tag  = r.tag;
        before.createdAt = s.startedAt;
        after.createdAt  = s.endedAt;
        checkIns.insert(before);
        checkIns.insert(after);
    }
}

void seedPlan(focus::Database& db) {
    focus::TaskRepository tasks(db);
    const std::string today     = QDate::currentDate().toString(Qt::ISODate).toStdString();
    const std::string yesterday = QDate::currentDate().addDays(-1).toString(Qt::ISODate).toStdString();
    const std::int64_t now      = std::time(nullptr);

    auto add = [&](const char* title, focus::Priority p, int minutes, const std::string& day, focus::TaskStatus st) {
        focus::Task t;
        t.title = title; t.priority = p; t.estimateMinutes = minutes; t.day = day; t.status = st; t.createdAt = now;
        if (st == focus::TaskStatus::Done) t.completedAt = now;
        return tasks.insert(t);
    };
    using P = focus::Priority;
    using S = focus::TaskStatus;
    add("Review the pull request",        P::Should, 30, yesterday, S::Todo);   // left over from yesterday
    add("Write the release notes",        P::Must,   45, yesterday, S::Todo);
    add("Finish the login endpoint",      P::Must,   60, today,     S::Todo);
    add("Write unit tests for billing",   P::Must,   45, today,     S::Done);
    add("Read the Spring Security docs",  P::Should, 30, today,     S::Todo);
    add("Reply to emails",                P::Could,  20, today,     S::Todo);
    add("Rename the old config keys",     P::Could,  15, today,     S::Dropped);
}

// A believable day at the computer, one row per minute.
void seedActivity(focus::Database& db) {
    using C = focus::ActivityCategory;
    const std::string today = QDate::currentDate().toString(Qt::ISODate).toStdString();
    std::vector<focus::ActivityBucket> buckets;
    auto seg = [&](int fromHour, int fromMin, int toHour, int toMin, C category, const char* app) {
        for (int m = fromHour * 60 + fromMin; m < toHour * 60 + toMin; ++m) {
            buckets.push_back({today, m, category, category == C::Away ? "" : app, 60});
        }
    };
    seg(8, 40, 9, 5,    C::Neutral,     "outlook");
    seg(9, 5,  9, 50,   C::Focus,       "code");
    seg(9, 50, 9, 58,   C::AiAssistant, "chrome");
    seg(9, 58, 10, 35,  C::Focus,       "code");
    seg(10, 35, 10, 50, C::Distraction, "chrome");
    seg(10, 50, 12, 10, C::Focus,       "devenv");
    seg(12, 10, 12, 55, C::Away,        "");
    seg(12, 55, 13, 20, C::Neutral,     "slack");
    seg(13, 20, 13, 40, C::Focus,       "code");
    seg(13, 40, 13, 46, C::AiAssistant, "chrome");
    seg(13, 46, 14, 10, C::Focus,       "code");
    seg(14, 10, 14, 25, C::Distraction, "chrome");
    seg(14, 25, 15, 40, C::Focus,       "code");
    seg(15, 40, 16, 0,  C::AiAssistant, "chrome");
    seg(16, 0, 16, 30,  C::Neutral,     "explorer");
    focus::ActivityRepository(db).addSeconds(buckets);
}

int mustCount(const QVariantList& tasks) {
    int n = 0;
    for (const auto& v : tasks) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("priority")).toInt() == 1 && !m.value(QStringLiteral("dropped")).toBool()) ++n;
    }
    return n;
}

int taskIdByTitle(const QVariantList& tasks, const QString& title) {
    for (const auto& v : tasks) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("title")).toString() == title) return m.value(QStringLiteral("taskId")).toInt();
    }
    return 0;
}

QVariantMap taskById(const QVariantList& tasks, int id) {
    for (const auto& v : tasks) {
        if (v.toMap().value(QStringLiteral("taskId")).toInt() == id) return v.toMap();
    }
    return {};
}

void shot(QQuickWindow* window, const QString& dir, const char* name) {
    if (dir.isEmpty()) return;
    const QImage image = window->grabWindow();
    check(!image.isNull() && image.width() > 100, name);
    image.save(QDir(dir).filePath(QString::fromLatin1(name) + QStringLiteral(".png")));
}

}  // namespace

int runSelfTest(QApplication&, const QString& screenshotDir) {
    g_previousHandler = qInstallMessageHandler(messageHandler);
    if (!screenshotDir.isEmpty()) QDir().mkpath(screenshotDir);

    // In-memory database + no real window detection: the test never touches your data.
    FocusBackend backend(":memory:", std::make_unique<focus::NullActivitySource>(), /*persistSettings=*/false);
    // Freeze "what time is it" for water and meals at 09:50, so the test does not depend on when it runs.
    backend.setLocalTimeForTesting(QDateTime(QDate::currentDate(), QTime(9, 50)));
    QStringList notes;
    QObject::connect(&backend, &FocusBackend::notification, [&notes](const QString& title, const QString&) { notes << title; });
    seedHistory(backend.database());
    seedPlan(backend.database());
    seedActivity(backend.database());
    backend.refreshStats();
    backend.refreshTasks();
    backend.refreshActivity();

    QQmlApplicationEngine engine;
    auto* window = qobject_cast<QQuickWindow*>(loadMainWindow(engine, backend));
    check(window != nullptr, "Main.qml loads");
    if (!window) return 1;
    window->resize(1200, 720);
    window->show();
    wait(1600);  // let the entrance animations finish

    check(backend.phase() == QLatin1String("idle"), "starts idle");
    check(backend.stats().value(QStringLiteral("completed")).toInt() == 7, "history shows 7 completed sessions");
    check(backend.tasks().size() == 5, "today has 5 tasks (one dropped)");
    check(backend.plan().value(QStringLiteral("done")).toInt() == 1, "one task is already done");
    check(backend.carryOverCount() == 2, "two unfinished tasks wait from yesterday");
    shot(window, screenshotDir, "1-today-dark");

    // ---- the must-do limit -------------------------------------------------------------------
    check(backend.addTask(QStringLiteral("Third must-do"), 1, 30).isEmpty(), "a third must-do is accepted");
    check(!backend.addTask(QStringLiteral("One must-do too many"), 1, 30).isEmpty(), "a fourth must-do is refused with a reason");
    check(!backend.addTask(QString(), 2, 30).isEmpty(), "a task without a name is refused");
    check(backend.addTask(QStringLiteral("A should-do is still fine"), 2, 25).isEmpty(), "other priorities are not limited");

    const int emailsId = taskIdByTitle(backend.tasks(), QStringLiteral("Reply to emails"));
    const QString msg  = backend.cyclePriority(emailsId);  // Could -> Must is refused, so it lands on Should
    check(!msg.isEmpty(), "cycling past a full must-do list explains why");
    check(taskById(backend.tasks(), emailsId).value(QStringLiteral("priority")).toInt() == 2, "priority skipped to Should");

    // ---- carrying over ---------------------------------------------------------------------------
    check(backend.carryOver() == 2, "two tasks carried over to today");
    check(backend.carryOverCount() == 0, "nothing left to carry over");
    check(mustCount(backend.tasks()) <= 3, "the must-do limit still holds after carrying over");
    wait(900);
    shot(window, screenshotDir, "1b-today-planned");

    backend.setTaskDone(taskIdByTitle(backend.tasks(), QStringLiteral("Read the Spring Security docs")), true);
    wait(700);

    window->setProperty("tab", QStringLiteral("insights"));
    wait(900);
    shot(window, screenshotDir, "2-insights");
    window->setProperty("tab", QStringLiteral("today"));
    wait(600);

    backend.setDarkMode(false);
    wait(700);
    shot(window, screenshotDir, "3-today-light");
    backend.setDarkMode(true);
    backend.setAccent(QStringLiteral("#14B8A6"));
    wait(600);
    shot(window, screenshotDir, "3b-today-teal");
    backend.setAccent(QStringLiteral("#7C5CFF"));

    // ---- water and meals --------------------------------------------------------------------
    check(notes.contains(QStringLiteral("Time for water")), "at 09:50 with no water logged, a water reminder fires");
    check(backend.wellness().value(QStringLiteral("glasses")).toInt() == 0, "no glasses logged yet");
    backend.logWater();
    backend.logWater();
    backend.logWater();
    check(backend.wellness().value(QStringLiteral("glasses")).toInt() == 3, "three glasses logged");
    backend.undoWater();
    check(backend.wellness().value(QStringLiteral("glasses")).toInt() == 2, "undo removes one");
    backend.setWaterGoal(10);
    check(backend.wellness().value(QStringLiteral("goal")).toInt() == 10, "the water goal can be changed");
    backend.setWaterGoal(8);

    backend.setLocalTimeForTesting(QDateTime(QDate::currentDate(), QTime(12, 40)));  // lunch window is open
    backend.advanceClockForTesting(60);
    backend.tickNow();
    backend.refreshWellness();
    check(notes.contains(QStringLiteral("Lunch time")), "a meal reminder fires when its window is open");
    const auto mealsBefore = backend.wellness().value(QStringLiteral("meals")).toList();
    check(mealsBefore.size() == 3 && mealsBefore[1].toMap().value(QStringLiteral("state")).toString() == QLatin1String("due"), "lunch is due");
    backend.setMeal(QStringLiteral("lunch"), QStringLiteral("ate"));
    const auto mealsAfter = backend.wellness().value(QStringLiteral("meals")).toList();
    check(mealsAfter[1].toMap().value(QStringLiteral("state")).toString() == QLatin1String("ate"), "lunch can be marked as eaten");
    backend.setMeal(QStringLiteral("dinner"), QStringLiteral("ate"));
    backend.setMeal(QStringLiteral("dinner"), QStringLiteral("clear"));
    backend.logWater();
    backend.setMeal(QStringLiteral("lunch"), QStringLiteral("clear"));  // leave lunch "due" for the screenshot

    window->setProperty("tab", QStringLiteral("health"));
    wait(1500);
    shot(window, screenshotDir, "8-health");
    window->setProperty("tab", QStringLiteral("today"));
    wait(500);

    // ---- all-day activity ----------------------------------------------------------------------
    backend.refreshActivity();
    const auto day = backend.activityDay();
    check(day.value(QStringLiteral("activeMinutes")).toDouble() > 300, "the day shows hours of activity");
    check(day.value(QStringLiteral("longestFocus")).toInt() >= 60, "there is a long focus block");
    check(day.value(QStringLiteral("aiMinutes")).toDouble() > 25, "AI time is counted");
    check(day.value(QStringLiteral("awayMinutes")).toDouble() >= 44, "the lunch break shows as away");
    check(day.value(QStringLiteral("hours")).toList().size() == 24, "24 hour columns");
    check(!day.value(QStringLiteral("apps")).toList().isEmpty() &&
          day.value(QStringLiteral("apps")).toList().first().toMap().value(QStringLiteral("app")).toString() == QLatin1String("code"),
          "code is the most used app");
    window->setProperty("tab", QStringLiteral("activity"));
    wait(1700);
    shot(window, screenshotDir, "9-activity");

    backend.setTrackingEnabled(false);
    check(!backend.trackingEnabled(), "tracking can be paused");
    backend.setTrackingEnabled(true);
    check(backend.trackingEnabled(), "and resumed");
    backend.clearTodayActivity();
    check(backend.activityDay().value(QStringLiteral("activeMinutes")).toDouble() < 1.0, "today's activity can be deleted");
    wait(500);
    shot(window, screenshotDir, "9b-activity-empty");
    window->setProperty("tab", QStringLiteral("today"));
    wait(500);

    // ---- start a session from a planned task ----------------------------------------------
    const int loginId = taskIdByTitle(backend.tasks(), QStringLiteral("Finish the login endpoint"));
    QMetaObject::invokeMethod(window, "openStartSheetFor", Q_ARG(QVariant, loginId),
                              Q_ARG(QVariant, QStringLiteral("Finish the login endpoint")), Q_ARG(QVariant, 60));
    wait(800);
    shot(window, screenshotDir, "4-start-sheet");

    check(!backend.startSession(QString(), QStringLiteral("x"), 25, 3, 3, 3, QString()), "a session without a goal is refused");
    check(backend.startSession(QStringLiteral("Finish the login endpoint"), QStringLiteral("Sketch it on paper"), 25, 3, 2, 4,
                               QStringLiteral("eager"), loginId),
          "session starts, linked to the task");
    wait(900);
    check(backend.phase() == QLatin1String("focusing"), "phase is focusing");

    backend.advanceClockForTesting(9 * 60 + 20);
    backend.logAiUse();
    backend.logAiUse();
    backend.tickNow();
    wait(1400);
    check(backend.aiUsed() == 2, "AI uses are counted");
    shot(window, screenshotDir, "5-session");

    backend.advanceClockForTesting(20 * 60);  // past the end of the 25 minute session
    backend.tickNow();
    wait(1200);
    check(backend.phase() == QLatin1String("checkin"), "session ends and asks for a check-in");
    check(backend.summary().value(QStringLiteral("taskId")).toInt() == loginId, "the check-in knows which task it worked on");
    shot(window, screenshotDir, "6-checkin");

    backend.submitCheckIn(4, 3, QStringLiteral("proud"));
    backend.setTaskDone(loginId, true);
    wait(1200);
    check(backend.phase() == QLatin1String("idle"), "back to idle after the check-in");
    check(backend.stats().value(QStringLiteral("completed")).toInt() == 8, "new session appears in the stats");
    const auto login = taskById(backend.tasks(), loginId);
    check(login.value(QStringLiteral("actualMinutes")).toInt() >= 24, "the session's time landed on the task");
    check(login.value(QStringLiteral("done")).toBool(), "the task is ticked off");
    shot(window, screenshotDir, "7-today-after");

    check(g_qmlWarnings == 0, "QML printed no warnings or errors");
    std::printf("%s\n", g_failures == 0 ? "SELF-TEST PASSED" : "SELF-TEST FAILED");
    return g_failures == 0 ? 0 : 1;
}
