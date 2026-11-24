#include <doctest/doctest.h>

#include <vector>

#include "core/SessionManager.h"

using namespace std::chrono_literals;
using namespace focus;
using Type = SessionEvent::Type;

namespace {
const SessionManager::Clock::time_point t0{};

SessionPlan plan(std::chrono::minutes d = 10min, std::optional<int> budget = std::nullopt) {
    SessionPlan p;
    p.goal     = "write the parser";
    p.approach = "read the spec first";
    p.duration = d;
    p.aiBudget = budget;
    return p;
}

struct Harness {
    std::vector<Type> events;
    SessionManager sm{[this](const SessionEvent& e) { events.push_back(e.type); }};

    int count(Type t) const {
        int n = 0;
        for (auto e : events) n += (e == t);
        return n;
    }
};
}  // namespace

TEST_CASE("a session needs a goal and a positive duration") {
    Harness h;
    SessionPlan p = plan();
    p.goal.clear();
    CHECK_FALSE(h.sm.start(p, t0));
    CHECK_FALSE(h.sm.isActive());

    CHECK_FALSE(h.sm.start(plan(0min), t0));
    CHECK(h.sm.start(plan(), t0));
    CHECK_FALSE(h.sm.start(plan(), t0));  // already running
}

TEST_CASE("session completes when the timer expires") {
    Harness h;
    h.sm.start(plan(10min), t0);
    h.sm.tick(t0 + 9min);
    CHECK(h.sm.state() == SessionState::Focusing);
    h.sm.tick(t0 + 10min);
    CHECK(h.sm.state() == SessionState::Idle);
    CHECK(h.count(Type::Completed) == 1);
    CHECK(h.sm.current().outcome == SessionOutcome::Completed);
    CHECK(h.sm.current().activeSeconds == 600);
}

TEST_CASE("paused time does not count toward the session") {
    Harness h;
    h.sm.start(plan(10min), t0);
    h.sm.pause(t0 + 4min);
    h.sm.tick(t0 + 100min);  // would be long expired if the pause were ignored
    CHECK(h.sm.state() == SessionState::Paused);
    h.sm.resume(t0 + 100min);
    h.sm.tick(t0 + 105min);
    CHECK(h.sm.isActive());
    h.sm.tick(t0 + 106min);
    CHECK(h.count(Type::Completed) == 1);
}

TEST_CASE("time is credited to the category you were in") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::Focus, t0 + 10s);        // first 10s were Neutral
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 70s);  // 60s of Focus
    h.sm.abandon(t0 + 100s);                                       // 30s of AI
    const auto& s = h.sm.current();
    CHECK(s.secondsIn(ActivityCategory::Neutral) == 10);
    CHECK(s.secondsIn(ActivityCategory::Focus) == 60);
    CHECK(s.secondsIn(ActivityCategory::AiAssistant) == 30);
    CHECK(s.outcome == SessionOutcome::Abandoned);
    CHECK(h.count(Type::Abandoned) == 1);
}

TEST_CASE("sub-second remainders are not lost between samples") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::Focus, t0);
    for (int i = 1; i <= 10; ++i) h.sm.recordActivity(ActivityCategory::Focus, t0 + 900ms * i);
    h.sm.abandon(t0 + 9s);
    CHECK(h.sm.current().secondsIn(ActivityCategory::Focus) == 9);  // truncating each sample would give 0
}

TEST_CASE("an AI visit is a switch into an AI tool, not each sample inside it") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 10s);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 20s);  // same visit
    h.sm.recordActivity(ActivityCategory::Focus, t0 + 30s);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 40s);  // second visit
    CHECK(h.sm.current().aiVisits == 2);
    CHECK(h.count(Type::AiVisit) == 2);
}

TEST_CASE("the AI budget warning fires once, when the limit is passed") {
    Harness h;
    h.sm.start(plan(10min, 1), t0);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 10s);  // 1 of 1: allowed
    CHECK(h.count(Type::AiBudgetExceeded) == 0);
    h.sm.logManualAiUse(t0 + 20s);  // 2 of 1: over budget
    CHECK(h.count(Type::AiBudgetExceeded) == 1);
    h.sm.logManualAiUse(t0 + 30s);
    CHECK(h.count(Type::AiBudgetExceeded) == 1);  // not repeated
    CHECK(h.sm.current().aiTotal() == 3);
}

TEST_CASE("no budget means no warning") {
    Harness h;
    h.sm.start(plan(10min, std::nullopt), t0);
    for (int i = 0; i < 5; ++i) h.sm.logManualAiUse(t0 + 1s);
    CHECK(h.count(Type::AiBudgetExceeded) == 0);
}

TEST_CASE("going away moves the whole idle stretch to Away, including time already credited") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::Focus, t0 + 1s);   // 1s neutral
    h.sm.recordActivity(ActivityCategory::Focus, t0 + 600s); // 599s focus so far
    h.sm.recordAway(300s, t0 + 602s);  // idle for 5 min: those 300s were never really focus
    h.sm.recordAway(302s, t0 + 604s);  // still away: only the new 2s are credited to Away
    h.sm.recordActivity(ActivityCategory::Focus, t0 + 606s);  // back at the keyboard

    const auto& s = h.sm.current();
    CHECK(s.secondsIn(ActivityCategory::Neutral) == 1);
    CHECK(s.secondsIn(ActivityCategory::Focus) == 301);
    CHECK(s.secondsIn(ActivityCategory::Away) == 304);
    // Nothing is lost or invented: the categories add up to the time that passed.
    CHECK(s.secondsIn(ActivityCategory::Neutral) + s.secondsIn(ActivityCategory::Focus) +
              s.secondsIn(ActivityCategory::Away) == 606);
}

TEST_CASE("the idle stretch cannot take more time than the category has") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::Focus, t0);
    h.sm.recordAway(3600s, t0 + 60s);  // idle for an hour, but the session is only a minute old
    CHECK(h.sm.current().secondsIn(ActivityCategory::Focus) == 0);
    CHECK(h.sm.current().secondsIn(ActivityCategory::Away) == 60);
}

TEST_CASE("coming back to the same AI window is not a new AI visit") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 10s);  // visit 1
    h.sm.recordAway(300s, t0 + 400s);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 500s);  // same window, after a break
    CHECK(h.sm.current().aiVisits == 1);

    h.sm.recordActivity(ActivityCategory::Focus, t0 + 510s);
    h.sm.recordActivity(ActivityCategory::AiAssistant, t0 + 520s);  // genuinely left and returned
    CHECK(h.sm.current().aiVisits == 2);
}

TEST_CASE("away is ignored while paused") {
    Harness h;
    h.sm.start(plan(), t0);
    h.sm.pause(t0 + 10s);
    h.sm.recordAway(300s, t0 + 400s);
    CHECK(h.sm.current().secondsIn(ActivityCategory::Away) == 0);
}
