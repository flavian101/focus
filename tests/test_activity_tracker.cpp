#include <doctest/doctest.h>

#include <algorithm>

#include "core/ActivityTracker.h"

using namespace std::chrono_literals;
using namespace focus;
using Cat = ActivityCategory;

namespace {
const ActivityTracker::Clock::time_point t0{};
LocalTime at(int minute, const char* day = "2026-11-23") { return LocalTime{day, minute}; }

int secondsFor(const std::vector<ActivityBucket>& b, Cat c, const std::string& app = "") {
    int total = 0;
    for (const auto& x : b) if (x.category == c && (app.empty() || x.app == app)) total += x.seconds;
    return total;
}

int activeFor(const std::vector<ActivityBucket>& b, Cat c, const std::string& app = "") {
    int total = 0;
    for (const auto& x : b) if (x.category == c && (app.empty() || x.app == app)) total += x.activeSeconds;
    return total;
}
}  // namespace

TEST_CASE("tracker: time is credited to the previous state (sample and hold)") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(600), Cat::Distraction, "chrome");   // the 5 s before this were "code"
    t.record(t0 + 10s, at(600), Cat::Distraction, "chrome");
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "code") == 5);
    CHECK(secondsFor(b, Cat::Distraction, "chrome") == 5);
}

TEST_CASE("tracker: a long gap (sleep, lock screen) is not credited to anything") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 2h, at(720), Cat::Focus, "code");  // the laptop slept for two hours
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus) == 15);  // capped at maxGap, not 7200
}

TEST_CASE("tracker: seconds are filed under the minute the state started in") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(601), Cat::Focus, "code");
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].minuteOfDay == 600);
}

TEST_CASE("tracker: going away takes the idle stretch back from the app that was wrongly credited") {
    ActivityTracker t;
    // ten minutes of focus, sampled every 5 s
    for (int i = 0; i <= 120; ++i) t.record(t0 + std::chrono::seconds(i * 5), at(600 + i * 5 / 60), Cat::Focus, "code");
    // then 5 minutes with no input: the first away sample arrives once idle reaches 300 s
    t.recordAway(t0 + 605s, at(610), 300s);
    t.recordAway(t0 + 610s, at(610), 305s);  // still away: only new time is credited, nothing more is moved

    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Away) == 300 + 5);
    CHECK(secondsFor(b, Cat::Focus, "code") == 605 - 300);     // everything before was focus, minus the idle stretch
    CHECK(secondsFor(b, Cat::Focus) + secondsFor(b, Cat::Away) == 610);  // nothing lost or invented
}

TEST_CASE("tracker: the idle stretch cannot take more than was recorded") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 10s, at(600), Cat::Focus, "code");
    t.recordAway(t0 + 15s, at(600), 3600s);  // claims an hour of idleness; only 15 s exist
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus) == 0);
    CHECK(secondsFor(b, Cat::Away) == 15);
}

TEST_CASE("tracker: only final buckets are handed out for saving") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(600), Cat::Focus, "code");
    t.record(t0 + 10s, at(606), Cat::Focus, "code");
    t.record(t0 + 15s, at(606), Cat::Focus, "code");

    auto early = t.takeFlushable(at(606), 6);  // minute 600 is exactly 6 minutes old: final
    CHECK(early.size() == 1);
    CHECK(early[0].minuteOfDay == 600);
    CHECK(t.pending().size() == 1);            // minute 606 is still being written
}

TEST_CASE("tracker: buckets from an earlier day are always final") {
    ActivityTracker t;
    t.record(t0, at(1439, "2026-11-23"), Cat::Focus, "code");
    t.record(t0 + 5s, at(1439, "2026-11-23"), Cat::Focus, "code");
    t.record(t0 + 10s, at(0, "2026-11-24"), Cat::Focus, "code");
    auto out = t.takeFlushable(at(0, "2026-11-24"), 6);
    REQUIRE(out.size() == 1);
    CHECK(out[0].day == "2026-11-23");
}

TEST_CASE("tracker: forgetting skips the pause, and a day can be discarded") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.forget();                                              // tracking was paused here
    t.record(t0 + 10s, at(600), Cat::Focus, "code");         // nothing is credited for the pause
    CHECK(t.pending().empty());

    t.record(t0 + 15s, at(601), Cat::Focus, "code");
    CHECK(t.pending().size() == 1);
    t.discardDay("2026-11-23");
    CHECK(t.pending().empty());
}

TEST_CASE("tracker: recategorizing an app moves its buffered time, and merges with what is there") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Neutral, "figma");
    t.record(t0 + 5s, at(600), Cat::Focus, "figma");        // 5 s neutral
    t.record(t0 + 10s, at(600), Cat::Neutral, "figma");     // 5 s focus
    t.record(t0 + 15s, at(600), Cat::Neutral, "slack");     // 5 s neutral again
    t.recategorize("2026-11-23", "figma", std::nullopt, Cat::Focus);
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "figma") == 15);        // all 15 s of figma (5 + 5 + 5), merged into one bucket
    CHECK(secondsFor(b, Cat::Neutral, "figma") == 0);
    CHECK(b.size() == 1);
}

TEST_CASE("tracker: recategorizing can be limited to one category (for browsers)") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Neutral, "chrome");
    t.record(t0 + 5s, at(600), Cat::AiAssistant, "chrome");
    t.record(t0 + 10s, at(600), Cat::Neutral, "chrome");
    t.recategorize("2026-11-23", "chrome", Cat::Neutral, Cat::Focus);   // only the neutral part
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "chrome") == 5);
    CHECK(secondsFor(b, Cat::AiAssistant, "chrome") == 5);              // the AI chat time is untouched
}

TEST_CASE("tracker: away time is never recategorized") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Away, "");
    t.record(t0 + 5s, at(600), Cat::Away, "");
    t.recategorize("2026-11-23", "", std::nullopt, Cat::Focus);
    CHECK(secondsFor(t.takeAll(), Cat::Away) == 5);
}

TEST_CASE("tracker: recategorizing also relabels the sample that is still being held") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Neutral, "figma");
    t.recategorize("2026-11-23", "figma", std::nullopt, Cat::Focus);   // the user just said "figma is focus"
    t.record(t0 + 5s, at(600), Cat::Focus, "figma");                    // the held 5 s must not come out as Neutral
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "figma") == 5);
    CHECK(secondsFor(b, Cat::Neutral, "figma") == 0);
}

// ---------------------------------------------------------------------------------- engagement

TEST_CASE("tracker: input during an interval is credited to the state that was held, like seconds") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(600), Cat::Focus, "code", SampleSignals{.input = true});   // typed during these 5 s
    t.record(t0 + 10s, at(600), Cat::Focus, "code", SampleSignals{.input = false}); // nothing during these 5 s
    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "code") == 10);
    CHECK(activeFor(b, Cat::Focus, "code") == 5);
}

TEST_CASE("tracker: with no signals nothing is engaged, so older callers behave as before") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(600), Cat::Focus, "code");
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].seconds == 5);
    CHECK(b[0].activeSeconds == 0);
    CHECK(b[0].switches == 0);
    CHECK(b[0].scrollEvents == 0);
}

TEST_CASE("tracker: a switch and the input counts are filed under the app you were in when they happened") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    SampleSignals s;
    s.input = true; s.switches = 1; s.scrollEvents = 7; s.keyEvents = 40; s.clickEvents = 2;
    t.record(t0 + 5s, at(600), Cat::Distraction, "chrome", s);   // you left code for chrome during that interval
    auto b = t.takeAll();

    REQUIRE(b.size() == 1);                       // chrome has not been credited yet: it is the held sample
    CHECK(b[0].app == "code");
    CHECK(b[0].switches == 1);
    CHECK(b[0].scrollEvents == 7);
    CHECK(b[0].keyEvents == 40);
    CHECK(b[0].clickEvents == 2);
}

TEST_CASE("tracker: counts add up within a minute") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 5s, at(600), Cat::Focus, "code", SampleSignals{.input = true, .scrollEvents = 2, .keyEvents = 10});
    t.record(t0 + 10s, at(600), Cat::Focus, "code", SampleSignals{.input = true, .scrollEvents = 3, .keyEvents = 5});
    t.record(t0 + 15s, at(600), Cat::Focus, "code");
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].scrollEvents == 5);
    CHECK(b[0].keyEvents == 15);
    CHECK(b[0].activeSeconds == 10);
    CHECK(b[0].seconds == 15);
}

TEST_CASE("tracker: after a long gap nothing about engagement is believed") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Focus, "code");
    t.record(t0 + 2h, at(720), Cat::Focus, "code", SampleSignals{.input = true, .switches = 3, .scrollEvents = 9});
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].seconds == 15);          // the usual cap
    CHECK(b[0].activeSeconds == 0);     // the laptop slept: no one was typing
    CHECK(b[0].switches == 0);
    CHECK(b[0].scrollEvents == 0);
}

TEST_CASE("tracker: Away is never engaged, whatever the caller says") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Away, "");
    t.record(t0 + 5s, at(600), Cat::Away, "", SampleSignals{.input = true, .keyEvents = 5});
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].seconds == 5);
    CHECK(b[0].activeSeconds == 0);
    CHECK(b[0].keyEvents == 0);
}

TEST_CASE("tracker: taking an idle stretch back from an app also takes back its input time") {
    ActivityTracker t;
    // ten minutes with input in every interval
    for (int i = 0; i <= 120; ++i) {
        t.record(t0 + std::chrono::seconds(i * 5), at(600 + i * 5 / 60), Cat::Focus, "code", SampleSignals{.input = true});
    }
    t.recordAway(t0 + 605s, at(610), 300s);

    auto b = t.takeAll();
    CHECK(secondsFor(b, Cat::Focus, "code") == 305);
    CHECK(activeFor(b, Cat::Focus, "code") == 305);   // what is left is the part where there really was input
    CHECK(activeFor(b, Cat::Away) == 0);
    for (const auto& x : b) CHECK(x.activeSeconds <= x.seconds);   // input never covers more time than exists
}

TEST_CASE("tracker: re-labelling an app keeps its engagement with the time") {
    ActivityTracker t;
    t.record(t0, at(600), Cat::Neutral, "figma");
    t.record(t0 + 5s, at(600), Cat::Neutral, "figma", SampleSignals{.input = true, .scrollEvents = 3});
    t.record(t0 + 10s, at(600), Cat::Neutral, "figma", SampleSignals{.input = false});
    t.recategorize("2026-11-23", "figma", std::nullopt, Cat::Focus);
    auto b = t.takeAll();
    REQUIRE(b.size() == 1);
    CHECK(b[0].category == Cat::Focus);
    CHECK(b[0].seconds == 10);
    CHECK(b[0].activeSeconds == 5);
    CHECK(b[0].scrollEvents == 3);
}
