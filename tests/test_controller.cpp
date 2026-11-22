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
