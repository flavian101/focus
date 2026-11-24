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
