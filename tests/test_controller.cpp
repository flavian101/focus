#include <doctest/doctest.h>

#include <algorithm>

#include "controller/SessionController.h"
#include "storage/ActivityRepository.h"
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
    InputCounts counts;        // what the "keyboard and mouse" did since the controller last asked
    bool counting = false;     // whether the controller has asked this source to count input events
    std::optional<ForegroundWindow> foreground() override { return window; }
    std::chrono::seconds idleFor() override { return idle; }
    void setInputCounting(bool enabled) override { counting = enabled; }
    InputCounts takeInputCounts() override {
        const InputCounts taken = counts;
        counts                  = {};
        return taken;
    }
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

// ------------------------------------------------------------------------------ water and meals

namespace {
LocalTime local(int minute, const char* day = "2026-11-23") { return LocalTime{day, minute}; }
}  // namespace

TEST_CASE("wellness: glasses can be logged and undone, and the snapshot shows pace") {
    Rig rig;
    rig.controller.logWater(local(9 * 60));
    rig.controller.logWater(local(10 * 60));
    auto snap = rig.controller.wellness(local(15 * 60));
    CHECK(snap.glasses == 2);
    CHECK(snap.goal == 8);
    CHECK(snap.expected == 4);  // 15:00 is halfway through 08:00-22:00

    CHECK(rig.controller.undoWater(local(15 * 60)));
    CHECK(rig.controller.wellness(local(15 * 60)).glasses == 1);
}

TEST_CASE("wellness: the water goal is saved and survives a restart") {
    Rig rig;
    rig.controller.setWaterGoal(10);
    CHECK(rig.controller.wellness(local(12 * 60)).goal == 10);

    SessionController reopened(rig.db, rig.source, Configuration::defaults());  // a "restart" on the same database
    CHECK(reopened.wellness(local(12 * 60)).goal == 10);

    rig.controller.setWaterGoal(500);
    CHECK(rig.controller.wellness(local(12 * 60)).goal == 20);  // clamped, and the clamped value is what is stored
}

TEST_CASE("wellness: a water reminder fires when behind pace, then waits") {
    Rig rig;
    rig.controller.tickWellness(local(10 * 60), t0);
    CHECK(rig.notified("Time for water"));

    const auto before = rig.notifications.size();
    rig.controller.tickWellness(local(10 * 60 + 1), t0 + 10s);  // looked at too recently: ignored
    CHECK(rig.notifications.size() == before);
}

TEST_CASE("wellness: logging a glass quietens the reminder") {
    Rig rig;
    rig.controller.logWater(local(9 * 60 + 50));
    rig.controller.tickWellness(local(10 * 60), t0);
    CHECK_FALSE(rig.notified("Time for water"));  // just had one, and pace is only 1 glass
}

TEST_CASE("wellness: nothing is shown while away, and it appears on return") {
    Rig rig;
    rig.source.idle = 20min;
    rig.controller.tickWellness(local(10 * 60), t0);
    CHECK(rig.notifications.empty());

    rig.source.idle = 0s;
    rig.controller.tickWellness(local(10 * 60 + 1), t0 + 31s);
    CHECK(rig.notified("Time for water"));
}

TEST_CASE("wellness: meals are reminded, answered, and reflected in the snapshot") {
    Rig rig;
    rig.controller.logWater(local(12 * 60 + 25));
    rig.controller.tickWellness(local(12 * 60 + 30), t0);
    CHECK(rig.notified("Lunch time"));

    auto snap = rig.controller.wellness(local(12 * 60 + 30));
    REQUIRE(snap.meals.size() == 3);
    CHECK(snap.meals[0].state == MealState::Missed);    // breakfast window ended at 10:00
    CHECK(snap.meals[1].state == MealState::Due);       // lunch
    CHECK(snap.meals[2].state == MealState::Upcoming);  // dinner

    rig.controller.setMeal(local(12 * 60 + 40), "lunch", MealStatus::Ate);
    CHECK(rig.controller.wellness(local(12 * 60 + 40)).meals[1].state == MealState::Ate);
    rig.controller.setMeal(local(12 * 60 + 40), "lunch", std::nullopt);  // changed my mind
    CHECK(rig.controller.wellness(local(12 * 60 + 40)).meals[1].state == MealState::Due);
}

TEST_CASE("wellness: it can be switched off") {
    Rig rig;
    auto cfg            = Configuration::defaults();
    cfg.wellnessEnabled = false;
    cfg.remindersEnabled = false;
    std::vector<std::string> seen;
    SessionController quiet(rig.db, rig.source, cfg, [&](const Notification& n) { seen.push_back(n.title); });
    quiet.tickWellness(local(10 * 60), t0);
    CHECK(seen.empty());
}

// ------------------------------------------------------------------------------ all-day activity

namespace {
// Runs the tracker once every 5 s from minute `startMinute` for `minutes` minutes.
void trackFor(Rig& rig, int startMinute, int minutes, std::chrono::seconds& clockSeconds) {
    const int samples = minutes * 12;
    for (int i = 0; i < samples; ++i) {
        rig.controller.trackActivity(local(startMinute + (i * 5) / 60), t0 + clockSeconds);
        clockSeconds += 5s;
    }
}
int secondsIn(const DaySummary& s, ActivityCategory c) { return static_cast<int>(s.secondsByCategory[static_cast<std::size_t>(c)]); }
}  // namespace

TEST_CASE("activity: app names and categories are recorded, never window titles") {
    Rig rig;
    rig.source.window = ForegroundWindow{"Code.exe", "secret-plans.md - Visual Studio Code"};
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 10, clock);

    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(secondsIn(day, ActivityCategory::Focus) >= 590);  // ~10 minutes (the last sample is still "held")
    REQUIRE_FALSE(day.topApps.empty());
    CHECK(day.topApps[0].app == "code");                    // normalised process name
    for (const auto& a : day.topApps) CHECK(a.app.find("secret") == std::string::npos);
}

TEST_CASE("activity: finished minutes are saved, recent ones stay buffered, and both show in the summary") {
    Rig rig;
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 12, clock);  // 09:00 to 09:12; flushes happen as the clock passes the 6-minute hold-back

    CHECK_FALSE(ActivityRepository(rig.db).forDay("2026-11-23").empty());   // older minutes already on disk
    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(secondsIn(day, ActivityCategory::Focus) >= 700);                  // saved + buffered together

    rig.controller.flushActivity();
    CHECK(secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus) == secondsIn(day, ActivityCategory::Focus));
}

TEST_CASE("activity: an absence is moved from the app to Away, including the part already credited") {
    Rig rig;
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 10, clock);          // ten minutes at the keyboard
    rig.source.idle = 5min;                    // the idle counter has just reached the threshold
    rig.controller.trackActivity(local(9 * 60 + 10), t0 + clock);
    clock += 5s;
    rig.source.idle = 5min + 5s;
    rig.controller.trackActivity(local(9 * 60 + 10), t0 + clock);

    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(secondsIn(day, ActivityCategory::Away) >= 300);
    CHECK(secondsIn(day, ActivityCategory::Focus) <= 305);   // about 5 real minutes of work, not 10
}

TEST_CASE("activity: a long gap (laptop asleep) is not credited") {
    Rig rig;
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 1, clock);
    const auto before = secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus);
    rig.controller.trackActivity(local(11 * 60), t0 + 2h);  // two hours later
    rig.controller.flushActivity();
    CHECK(secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus) - before <= 15);
}

TEST_CASE("activity: pausing tracking records nothing, is remembered, and the pause is not credited") {
    Rig rig;
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 2, clock);
    rig.controller.setTrackingEnabled(false);
    const auto saved = secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus);
    trackFor(rig, 9 * 60 + 2, 30, clock);                    // thirty minutes while paused
    CHECK(secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus) == saved);

    SessionController reopened(rig.db, rig.source, Configuration::defaults());
    CHECK_FALSE(reopened.trackingEnabled());                 // the choice survived a restart

    rig.controller.setTrackingEnabled(true);
    rig.controller.trackActivity(local(10 * 60), t0 + clock);
    clock += 5s;
    rig.controller.trackActivity(local(10 * 60), t0 + clock);
    CHECK(secondsIn(rig.controller.daySummary("2026-11-23"), ActivityCategory::Focus) - saved <= 10);  // only the 5 s after resuming
}

TEST_CASE("activity: a day's data can be deleted, buffered minutes included") {
    Rig rig;
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 12, clock);
    rig.controller.clearActivity("2026-11-23");
    CHECK(rig.controller.daySummary("2026-11-23").activeSeconds == 0);
    CHECK(ActivityRepository(rig.db).forDay("2026-11-23").empty());
}

// ---------------------------------------------------------------- tracking modes, profile, rules

namespace {
// Records 5 minutes of Code.exe at the given day/time.
void trackAt(Rig& rig, const char* day, int minute, std::chrono::seconds& clock) {
    for (int i = 0; i < 60; ++i) {
        rig.controller.trackActivity(LocalTime{day, minute + (i * 5) / 60}, t0 + clock);
        clock += 5s;
    }
}
bool hasApp(const DaySummary& d, const std::string& app) {
    for (const auto& a : d.topApps) if (a.app == app) return true;
    return false;
}
}  // namespace

TEST_CASE("modes: during work hours the app name is kept") {
    Rig rig;  // default policy: Work Monday-Friday 09:00-17:00
    rig.source.window = ForegroundWindow{"Code.exe", "x"};
    std::chrono::seconds clock{0};
    trackAt(rig, "2026-11-25", 10 * 60, clock);  // a Wednesday morning
    auto day = rig.controller.daySummary("2026-11-25");
    CHECK(hasApp(day, "code"));
    CHECK(rig.controller.currentTrackingMode(LocalTime{"2026-11-25", 10 * 60}) == TrackingMode::Work);
}

TEST_CASE("modes: outside work hours only the category is kept, never the app") {
    Rig rig;
    rig.source.window = ForegroundWindow{"Code.exe", "x"};
    std::chrono::seconds clock{0};
    trackAt(rig, "2026-11-25", 22 * 60, clock);  // a Wednesday night
    auto day = rig.controller.daySummary("2026-11-25");
    CHECK(secondsIn(day, ActivityCategory::Focus) > 0);   // the time is known...
    CHECK_FALSE(hasApp(day, "code"));                      // ...but not which app
    for (const auto& a : day.topApps) CHECK(a.app.empty());
    CHECK(rig.controller.currentTrackingMode(LocalTime{"2026-11-25", 22 * 60}) == TrackingMode::Private);
}

TEST_CASE("modes: Off records nothing, keeps what was already recorded, and does not credit the gap") {
    Rig rig;
    TrackingPolicy p;
    p.otherwise = TrackingMode::Off;
    REQUIRE(rig.controller.setTrackingPolicy(p));

    std::chrono::seconds clock{0};
    trackAt(rig, "2026-11-25", 16 * 60, clock);                 // work hours: recorded
    const auto before = rig.controller.daySummary("2026-11-25").activeSeconds;
    trackAt(rig, "2026-11-25", 20 * 60, clock);                 // evening: Off
    CHECK(rig.controller.daySummary("2026-11-25").activeSeconds == before);
    CHECK(ActivityRepository(rig.db).forDay("2026-11-25").size() > 0);   // the earlier time was saved, not lost
}

TEST_CASE("modes: the pause switch beats the schedule") {
    Rig rig;
    rig.controller.setTrackingEnabled(false);
    CHECK(rig.controller.currentTrackingMode(LocalTime{"2026-11-25", 10 * 60}) == TrackingMode::Off);
}

TEST_CASE("modes: the policy and profile are saved, validated and survive a restart") {
    Rig rig;
    TrackingPolicy p;
    p.duringWork = TrackingMode::Work;
    p.otherwise  = TrackingMode::Off;
    p.schedule.days = 0b0111111;   // Saturday counts too
    p.schedule.startMinute = 8 * 60;
    p.schedule.endMinute   = 18 * 60;
    REQUIRE(rig.controller.setTrackingPolicy(p));

    TrackingPolicy bad = p;
    bad.schedule.startMinute = 19 * 60;   // starts after it ends
    CHECK_FALSE(rig.controller.setTrackingPolicy(bad));
    CHECK(rig.controller.trackingPolicy().schedule.startMinute == 8 * 60);  // unchanged

    rig.controller.setProfile({"  intern  ", "Mostly coding."});
    SessionController reopened(rig.db, rig.source, Configuration::defaults());
    CHECK(reopened.trackingPolicy().otherwise == TrackingMode::Off);
    CHECK(reopened.trackingPolicy().schedule.days == 0b0111111);
    CHECK(reopened.trackingPolicy().schedule.endMinute == 18 * 60);
    CHECK(reopened.profile().role == "intern");
    CHECK(reopened.profile().purpose == "Mostly coding.");
}

TEST_CASE("rules: a user override changes how the app is classified from now on, and today's recorded time") {
    Rig rig;
    rig.source.window = ForegroundWindow{"Figma.exe", "Design"};   // no built-in opinion: Neutral
    std::chrono::seconds clock{0};
    trackAt(rig, "2026-11-25", 10 * 60, clock);
    CHECK(secondsIn(rig.controller.daySummary("2026-11-25"), ActivityCategory::Neutral) > 0);

    rig.controller.setAppCategory("Figma.exe", ActivityCategory::Focus, "2026-11-25");
    auto day = rig.controller.daySummary("2026-11-25");
    CHECK(secondsIn(day, ActivityCategory::Neutral) == 0);          // today's time was re-labelled
    CHECK(secondsIn(day, ActivityCategory::Focus) > 0);

    trackAt(rig, "2026-11-25", 11 * 60, clock);                      // and new time uses the rule
    CHECK(secondsIn(rig.controller.daySummary("2026-11-25"), ActivityCategory::Neutral) == 0);

    SessionController reopened(rig.db, rig.source, Configuration::defaults());
    CHECK(reopened.appRules().at("figma") == ActivityCategory::Focus);   // saved

    rig.controller.clearAppCategory("figma");
    CHECK(rig.controller.appRules().empty());
}

TEST_CASE("rules: marking a browser as focus does not hide an AI chat in a tab") {
    Rig rig;
    rig.controller.setAppCategory("chrome.exe", ActivityCategory::Focus, "2026-11-25");
    std::chrono::seconds clock{0};
    rig.source.window = ForegroundWindow{"chrome.exe", "Docs - Google Chrome"};
    trackAt(rig, "2026-11-25", 10 * 60, clock);
    rig.source.window = ForegroundWindow{"chrome.exe", "Claude - Google Chrome"};
    trackAt(rig, "2026-11-25", 10 * 60 + 5, clock);
    auto day = rig.controller.daySummary("2026-11-25");
    CHECK(secondsIn(day, ActivityCategory::Focus) > 0);
    CHECK(secondsIn(day, ActivityCategory::AiAssistant) > 0);
}

TEST_CASE("rules: away cannot be assigned to an app") {
    Rig rig;
    rig.controller.setAppCategory("steam", ActivityCategory::Away, "2026-11-25");
    CHECK(rig.controller.appRules().empty());
}

// ---------------------------------------------------------------------------------- engagement

namespace {
void useMode(Rig& rig, TrackingMode duringWork) {
    TrackingPolicy p;
    p.duringWork = duringWork;
    REQUIRE(rig.controller.setTrackingPolicy(p));
}
}  // namespace

TEST_CASE("engagement: time at the keyboard is engaged, time just sitting in front of an app is not") {
    Rig rig;  // Monday 09:00 is inside the default work hours
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 2, clock);   // two minutes with input in every interval (idle stays at 0)
    rig.source.idle = 30s;             // then two minutes with the app in front and nobody touching anything
    trackFor(rig, 9 * 60 + 2, 2, clock);

    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(day.hasEngagementData);
    CHECK(secondsIn(day, ActivityCategory::Focus) >= 235);
    CHECK(day.engagedSeconds >= 110);
    CHECK(day.engagedSeconds <= 125);                              // about half of it: the first two minutes
    CHECK(day.engagedShare(ActivityCategory::Focus) < 0.55);
    CHECK(day.engagedShare(ActivityCategory::Focus) > 0.45);
}

TEST_CASE("engagement: a change of window or tab is counted, the same page again is not, and no title is kept") {
    Rig rig;
    std::chrono::seconds clock{0};
    for (int i = 0; i < 24; ++i) {   // two minutes, sampled every 5 s, flipping between two tabs of the same browser
        rig.source.window = ForegroundWindow{"msedge.exe", i % 2 == 0 ? "Inbox - Mail - Edge" : "Some private page - Edge"};
        rig.controller.trackActivity(local(9 * 60 + (i * 5) / 60), t0 + clock);
        clock += 5s;
    }
    auto flipping = rig.controller.daySummary("2026-11-23");
    CHECK(flipping.foregroundChanges == 23);   // every sample after the first was a different tab

    Rig calm;
    std::chrono::seconds calmClock{0};
    trackFor(calm, 9 * 60, 2, calmClock);      // the same window the whole time
    CHECK(calm.controller.daySummary("2026-11-23").foregroundChanges == 0);

    for (const auto& b : ActivityRepository(rig.db).forDay("2026-11-23")) {
        CHECK(b.app.find("Inbox") == std::string::npos);   // app names only: the titles were never stored
        CHECK(b.app.find("private") == std::string::npos);
    }
}

TEST_CASE("engagement: a moment with no foreground window does not turn one switch into two") {
    Rig rig;
    std::chrono::seconds clock{0};
    auto sample = [&](std::optional<ForegroundWindow> w) {
        rig.source.window = w;
        rig.controller.trackActivity(local(9 * 60), t0 + clock);
        clock += 5s;
    };
    sample(ForegroundWindow{"Code.exe", "a"});
    sample(std::nullopt);                          // mid alt-tab, Windows reports nothing
    sample(ForegroundWindow{"chrome.exe", "b"});   // one real switch
    sample(ForegroundWindow{"chrome.exe", "b"});
    CHECK(rig.controller.daySummary("2026-11-23").foregroundChanges == 1);
}

TEST_CASE("engagement: Work mode never listens for scrolls, keys or clicks") {
    Rig rig;
    useMode(rig, TrackingMode::Work);
    rig.source.counts = InputCounts{10, 20, 3};
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 1, clock);

    CHECK_FALSE(rig.source.counting);              // the source was never asked to count
    CHECK(rig.source.counts.scrolls == 10);        // and nothing was even read from it
    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(day.scrollEvents == 0);
    CHECK(day.keyEvents == 0);
    CHECK(day.clickEvents == 0);
    CHECK(day.engagedSeconds > 0);                 // the engagement basics are still there
}

TEST_CASE("engagement: Detailed mode listens, and the counts land in the day") {
    Rig rig;
    useMode(rig, TrackingMode::Detailed);
    std::chrono::seconds clock{0};
    for (int i = 0; i < 24; ++i) {
        rig.source.counts = InputCounts{2, 5, 1};   // what happened since the previous sample
        rig.controller.trackActivity(local(9 * 60 + (i * 5) / 60), t0 + clock);
        clock += 5s;
    }
    CHECK(rig.source.counting);
    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(day.scrollEvents == 46);   // 23 intervals: the first sample has no interval behind it
    CHECK(day.keyEvents == 115);
    CHECK(day.clickEvents == 23);
}

TEST_CASE("engagement: listening stops at once when tracking is paused or the mode drops below Detailed") {
    Rig rig;
    useMode(rig, TrackingMode::Detailed);
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 1, clock);
    REQUIRE(rig.source.counting);

    rig.controller.setTrackingEnabled(false);
    CHECK_FALSE(rig.source.counting);              // right away, without waiting for the next tick

    rig.controller.setTrackingEnabled(true);
    trackFor(rig, 9 * 60 + 1, 1, clock);
    CHECK(rig.source.counting);                    // back on, because the mode still allows it

    useMode(rig, TrackingMode::Work);
    rig.controller.trackActivity(local(9 * 60 + 2), t0 + clock);
    CHECK_FALSE(rig.source.counting);              // the next tick notices the lower level
}

TEST_CASE("engagement: input counts gathered while away are discarded, not credited to a later moment") {
    Rig rig;
    useMode(rig, TrackingMode::Detailed);
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 1, clock);

    rig.source.idle   = 6min;                     // past the away threshold
    rig.source.counts = InputCounts{9, 9, 9};
    rig.controller.trackActivity(local(9 * 60 + 1), t0 + clock);
    clock += 5s;
    CHECK(rig.source.counts.scrolls == 0);         // drained, even though they were not used

    rig.source.idle = 0s;
    trackFor(rig, 9 * 60 + 2, 1, clock);
    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(day.scrollEvents == 0);
    CHECK(day.keyEvents == 0);
}

TEST_CASE("engagement: Private mode keeps engagement but still never the app name, and no input counts") {
    Rig rig;
    useMode(rig, TrackingMode::Private);
    rig.source.counts = InputCounts{4, 4, 4};
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 2, clock);

    auto day = rig.controller.daySummary("2026-11-23");
    CHECK(day.hasEngagementData);
    CHECK(day.engagedSeconds > 0);
    CHECK(day.scrollEvents == 0);
    CHECK_FALSE(rig.source.counting);
    for (const auto& a : day.topApps) CHECK(a.app.empty());
}

TEST_CASE("engagement: Off records nothing at all, including engagement") {
    Rig rig;
    useMode(rig, TrackingMode::Off);
    std::chrono::seconds clock{0};
    trackFor(rig, 9 * 60, 2, clock);
    CHECK(rig.controller.daySummary("2026-11-23").hasEngagementData == false);
    CHECK_FALSE(rig.source.counting);
    CHECK(ActivityRepository(rig.db).forDay("2026-11-23").empty());
}

TEST_CASE("engagement: it survives being saved and reloaded, merged with what is still buffered") {
    Rig rig;
    useMode(rig, TrackingMode::Detailed);
    std::chrono::seconds clock{0};
    for (int i = 0; i < 12 * 12; ++i) {   // twelve minutes, so the older ones are flushed to the database
        rig.source.counts = InputCounts{1, 2, 0};
        rig.controller.trackActivity(local(9 * 60 + (i * 5) / 60), t0 + clock);
        clock += 5s;
    }
    const auto before = rig.controller.daySummary("2026-11-23");
    CHECK_FALSE(ActivityRepository(rig.db).forDay("2026-11-23").empty());   // some of it is already on disk

    rig.controller.flushActivity();
    const auto after = rig.controller.daySummary("2026-11-23");
    CHECK(after.scrollEvents == before.scrollEvents);
    CHECK(after.keyEvents == before.keyEvents);
    CHECK(after.engagedSeconds == before.engagedSeconds);
    CHECK(after.scrollEvents == 143);
}
