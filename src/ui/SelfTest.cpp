// Headless self-test: loads the real QML, walks through a whole session, checks the QML
// produced no warnings, and saves screenshots. Run via `ctest` or `focus_ui --self-test <dir>`.
#include <QApplication>
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
#include "storage/CheckInRepository.h"
#include "storage/SessionRepository.h"

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
    seedHistory(backend.database());
    backend.refreshStats();

    QQmlApplicationEngine engine;
    auto* window = qobject_cast<QQuickWindow*>(loadMainWindow(engine, backend));
    check(window != nullptr, "Main.qml loads");
    if (!window) return 1;
    window->resize(1080, 720);
    window->show();
    wait(1600);  // let the entrance animations finish

    check(backend.phase() == QLatin1String("idle"), "starts idle");
    check(backend.stats().value(QStringLiteral("completed")).toInt() == 7, "history shows 7 completed sessions");
    shot(window, screenshotDir, "1-dashboard-dark");

    backend.setDarkMode(false);
    wait(700);
    shot(window, screenshotDir, "2-dashboard-light");
    backend.setDarkMode(true);
    backend.setAccent(QStringLiteral("#14B8A6"));
    wait(600);
    shot(window, screenshotDir, "3-dashboard-teal");
    backend.setAccent(QStringLiteral("#7C5CFF"));

    QMetaObject::invokeMethod(window, "openStartSheet");
    wait(800);
    shot(window, screenshotDir, "4-start-sheet");

    check(!backend.startSession(QString(), QStringLiteral("x"), 25, 3, 3, 3, QString()), "a session without a goal is refused");
    check(backend.startSession(QStringLiteral("Write the self-test"), QStringLiteral("Sketch it on paper"), 25, 3, 2, 4, QStringLiteral("eager")),
          "session starts");
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
    shot(window, screenshotDir, "6-checkin");

    backend.submitCheckIn(4, 3, QStringLiteral("proud"));
    wait(1200);
    check(backend.phase() == QLatin1String("idle"), "back to idle after the check-in");
    check(backend.stats().value(QStringLiteral("completed")).toInt() == 8, "new session appears in the stats");
    shot(window, screenshotDir, "7-dashboard-after");

    check(g_qmlWarnings == 0, "QML printed no warnings or errors");
    std::printf("%s\n", g_failures == 0 ? "SELF-TEST PASSED" : "SELF-TEST FAILED");
    return g_failures == 0 ? 0 : 1;
}
