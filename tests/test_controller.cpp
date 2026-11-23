#include <doctest/doctest.h>

#include <algorithm>

#include "controller/SessionController.h"
#include "storage/CheckInRepository.h"
#include "storage/Migrations.h"
#include "storage/SessionRepository.h"

using namespace std::chrono_literals;
using namespace focus;

namespace {

const SessionController::Clock::time_point t0{};

struct FakeSource : ActivitySource {
    std::optional<ForegroundWindow> window = ForegroundWindow{"Code.exe", "main.cpp - Visual Studio Code"};
    std::chrono::seconds idle{0};
    std::optional<ForegroundWindow> foreground() override { return window; }
    std::chrono::seconds idleFor() override { return idle; }
};

struct Rig {
    Database db = [] {
        auto d = Database::open(":memory:");
        migrate(d);
        return d;
    }();
    FakeSource source;
    std::vector<std::string> notifications;  // titles, in order
    SessionController controller;

    explicit Rig(bool reminders = false)
        : controller(db, source, makeConfig(reminders),
                     [this](const Notification& n) { notifications.push_back(n.title); }) {}

    static Configuration makeConfig(bool reminders) {
        auto c             = Configuration::defaults();
        c.remindersEnabled = reminders;
        return c;
    }
    bool notified(const std::string& title) const {
        return std::find(notifications.begin(), notifications.end(), title) != notifications.end();
    }
};

SessionPlan plan(std::chrono::minutes d = 25min, std::optional<int> budget = 3) {
    SessionPlan p;
    p.goal     = "write the controller";
    p.approach = "sketch the API first";
    p.duration = d;
    p.aiBudget = budget;
    return p;
}

CheckIn mood(int m, int e, const char* tag = "") {
    CheckIn c;
    c.mood   = m;
    c.energy = e;
    c.tag    = tag;
    return c;
}

}  // namespace

TEST_CASE("controller: a full session is saved, then closed with a check-in") {
    Rig rig;
    REQUIRE(rig.controller.start(plan(), mood(2, 3, "stuck"), t0));
    CHECK(rig.controller.snapshot(t0).phase == Phase::Focusing);

    rig.controller.tick(t0 + 2s);     // samples: editor -> Focus
    rig.controller.tick(t0 + 25min);  // time is up
    auto snap = rig.controller.snapshot(t0 + 25min);
    CHECK(snap.phase == Phase::CheckIn);
    REQUIRE(snap.lastFinished.has_value());
    CHECK(snap.lastFinished->outcome == SessionOutcome::Completed);
    CHECK(snap.lastFinished->secondsIn(ActivityCategory::Focus) > 1400);
    CHECK(snap.progress == doctest::Approx(1.0));

    // Already on disk before the user answers the check-in:
    CHECK(SessionRepository(rig.db).recent(5).size() == 1);

    rig.controller.submitAfterCheckIn(mood(4, 3, "calm"));
    CHECK(rig.controller.snapshot(t0 + 26min).phase == Phase::Idle);

    auto report = rig.controller.stats(7);
    CHECK(report.sessionsCompleted == 1);
    REQUIRE(report.avgMoodChange.has_value());
    CHECK(*report.avgMoodChange == doctest::Approx(2.0));
}

TEST_CASE("controller: skipping the check-in keeps the session and the 'before' mood") {
    Rig rig;
    rig.controller.start(plan(), mood(3, 3), t0);
    rig.controller.abandon(t0 + 5min);
    CHECK(rig.controller.snapshot(t0 + 5min).phase == Phase::CheckIn);
    rig.controller.skipAfterCheckIn();
    CHECK(rig.controller.snapshot(t0 + 5min).phase == Phase::Idle);

    auto sessions = SessionRepository(rig.db).recent(5);
    REQUIRE(sessions.size() == 1);
    CHECK(sessions[0].outcome == SessionOutcome::Abandoned);
    auto checkIns = CheckInRepository(rig.db).createdSince(0);
    REQUIRE(checkIns.size() == 1);
    CHECK(checkIns[0].kind == CheckInKind::BeforeSession);
    CHECK(checkIns[0].sessionId == sessions[0].id);
}

TEST_CASE("controller: AI windows are detected, counted and trigger the nudges") {
    Rig rig;
    rig.source.window = ForegroundWindow{"chrome.exe", "Claude - Google Chrome"};
    rig.controller.start(plan(25min, 0), mood(3, 3), t0);  // budget of zero visits
    rig.controller.tick(t0 + 2s);
    CHECK(rig.controller.snapshot(t0 + 2s).aiUsed == 1);
    CHECK(rig.notified("AI visit 1 of 0"));
    CHECK(rig.notified("AI budget used up"));
}

TEST_CASE("controller: manual AI log counts against the budget") {
    Rig rig;
    rig.controller.start(plan(25min, 1), mood(3, 3), t0);
    rig.controller.logAiUse(t0 + 1min);
    rig.controller.logAiUse(t0 + 2min);
    CHECK(rig.controller.snapshot(t0 + 2min).aiUsed == 2);
    CHECK(rig.notified("AI budget used up"));
}

TEST_CASE("controller: idle time becomes Away and holds back reminders until you return") {
    Rig rig(true);  // reminders on: first one is Eye break at 20 min
    rig.controller.start(plan(60min), mood(3, 3), t0);
    rig.controller.tick(t0 + 2s);

    rig.source.idle = 10min;
    rig.controller.tick(t0 + 21min);  // eye break is due, but nobody is there
    CHECK_FALSE(rig.controller.snapshot(t0 + 21min).present);
    CHECK_FALSE(rig.notified("Eye break"));
    CHECK(rig.controller.snapshot(t0 + 21min).secondsByCategory[static_cast<std::size_t>(ActivityCategory::Away)] > 0);

    rig.source.idle = 0s;
    rig.controller.tick(t0 + 21min + 2s);  // back at the keyboard
    CHECK(rig.notified("Eye break"));
}

TEST_CASE("controller: pause and resume") {
    Rig rig;
    rig.controller.start(plan(), mood(3, 3), t0);
    rig.controller.pause(t0 + 1min);
    CHECK(rig.controller.snapshot(t0 + 10min).phase == Phase::Paused);
    CHECK(rig.controller.snapshot(t0 + 10min).remainingSeconds == 24 * 60);  // frozen
    rig.controller.resume(t0 + 10min);
    CHECK(rig.controller.snapshot(t0 + 11min).phase == Phase::Focusing);
}

TEST_CASE("controller: refuses bad input and a second session") {
    Rig rig;
    auto p = plan();
    p.goal.clear();
    CHECK_FALSE(rig.controller.start(p, mood(3, 3), t0));          // no goal
    CHECK_FALSE(rig.controller.start(plan(), mood(9, 3), t0));     // mood out of range
    CHECK(rig.controller.start(plan(), mood(3, 3), t0));
    CHECK_FALSE(rig.controller.start(plan(), mood(3, 3), t0));     // already running
}

TEST_CASE("controller: recent sessions come back newest first") {
    Rig rig;
    for (int i = 0; i < 3; ++i) {
        auto p = plan();
        p.goal = "goal " + std::to_string(i);
        rig.controller.start(p, mood(3, 3), t0);
        rig.controller.abandon(t0 + 1min);
        rig.controller.skipAfterCheckIn();
    }
    auto recent = rig.controller.recentSessions(2);
    REQUIRE(recent.size() == 2);
    CHECK(recent[0].goal == "goal 2");
}

// ------------------------------------------------------------------------------------ day plan

TEST_CASE("plan: tasks are added in priority order and the must-do limit is explained") {
    Rig rig;
    CHECK(rig.controller.addTask("2026-11-23", "could do", Priority::Could, 20).ok);
    for (int i = 0; i < 3; ++i) CHECK(rig.controller.addTask("2026-11-23", "must " + std::to_string(i), Priority::Must, 30).ok);

    auto fourth = rig.controller.addTask("2026-11-23", "one too many", Priority::Must, 30);
    CHECK_FALSE(fourth.ok);
    CHECK(fourth.message.find("3 must-dos") != std::string::npos);

    auto tasks = rig.controller.tasksFor("2026-11-23");
    REQUIRE(tasks.size() == 4);
    CHECK(tasks[0].priority == Priority::Must);
    CHECK(tasks[3].priority == Priority::Could);

    CHECK_FALSE(rig.controller.addTask("2026-11-23", "", Priority::Should, 20).ok);       // no name
    CHECK_FALSE(rig.controller.addTask("2026-11-23", "x", Priority::Should, 0).ok);       // no estimate
}

TEST_CASE("plan: promoting to Must respects the limit; completing and dropping update the summary") {
    Rig rig;
    std::vector<std::int64_t> musts;
    for (int i = 0; i < 3; ++i) musts.push_back(rig.controller.addTask("2026-11-23", "m" + std::to_string(i), Priority::Must, 30).id);
    const auto should = rig.controller.addTask("2026-11-23", "s", Priority::Should, 60).id;

    CHECK_FALSE(rig.controller.setTaskPriority(should, Priority::Must).ok);   // three musts already
    rig.controller.setTaskStatus(musts[0], TaskStatus::Dropped);               // frees a slot
    CHECK(rig.controller.setTaskPriority(should, Priority::Must).ok);

    rig.controller.setTaskStatus(musts[1], TaskStatus::Done);
    auto plan = rig.controller.planFor("2026-11-23");
    CHECK(plan.total == 3);
    CHECK(plan.done == 1);
    CHECK(plan.dropped == 1);
    CHECK(plan.mustDone == 1);
}

TEST_CASE("plan: a session linked to a task adds its time to the task") {
    Rig rig;
    const auto id = rig.controller.addTask("2026-11-23", "write the controller", Priority::Must, 25).id;

    auto p   = plan();
    p.taskId = id;
    REQUIRE(rig.controller.start(p, mood(3, 3), t0));
    rig.controller.tick(t0 + 25min);
    rig.controller.skipAfterCheckIn();

    auto tasks = rig.controller.tasksFor("2026-11-23");
    REQUIRE(tasks.size() == 1);
    CHECK(tasks[0].actualSeconds == 25 * 60);
    CHECK(rig.controller.planFor("2026-11-23").actualMinutes == 25);
}

TEST_CASE("plan: a missing task id does not lose the session") {
    Rig rig;
    auto p   = plan();
    p.taskId = 4242;  // no such task
    REQUIRE(rig.controller.start(p, mood(3, 3), t0));
    rig.controller.abandon(t0 + 1min);  // must not throw on a foreign-key failure
    auto saved = rig.controller.recentSessions(1);
    REQUIRE(saved.size() == 1);
    CHECK_FALSE(saved[0].taskId.has_value());
}

TEST_CASE("plan: unfinished tasks carry over, extras beyond the must-do limit become Should") {
    Rig rig;
    for (int i = 0; i < 2; ++i) rig.controller.addTask("2026-11-22", "old must " + std::to_string(i), Priority::Must, 30);
    const auto doneId = rig.controller.addTask("2026-11-22", "already done", Priority::Must, 30).id;
    rig.controller.setTaskStatus(doneId, TaskStatus::Done);
    rig.controller.addTask("2026-11-23", "today must A", Priority::Must, 30);
    rig.controller.addTask("2026-11-23", "today must B", Priority::Must, 30);

    CHECK(rig.controller.unfinishedBefore("2026-11-23") == 2);
    CHECK(rig.controller.carryOverUnfinished("2026-11-23") == 2);
    CHECK(rig.controller.carryOverUnfinished("2026-11-23") == 0);  // nothing left to move

    auto today = rig.controller.tasksFor("2026-11-23");
    CHECK(today.size() == 4);
    int musts = 0;
    for (const auto& t : today) musts += t.priority == Priority::Must;
    CHECK(musts == 3);  // one old must fit (2 + 1), the other was demoted to Should
}
