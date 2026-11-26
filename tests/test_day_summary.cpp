#include <doctest/doctest.h>

#include "core/DaySummary.h"

using namespace focus;
using Cat = ActivityCategory;

namespace {
// `count` full minutes starting at `from`, all in one category and app.
void minutes(std::vector<ActivityBucket>& out, int from, int count, Cat c, const char* app) {
    for (int m = from; m < from + count; ++m) out.push_back({"2026-11-23", m, c, c == Cat::Away ? "" : app, 60});
}
constexpr int at(int h, int m = 0) { return h * 60 + m; }
}  // namespace

TEST_CASE("day: an empty day is empty") {
    auto s = summarizeDay({});
    CHECK(s.activeSeconds == 0);
    CHECK(s.firstActiveMinute == -1);
    CHECK(s.longestFocusMinutes == 0);
    CHECK(s.switchesPerActiveHour == 0.0);
    CHECK(s.topApps.empty());
}

TEST_CASE("day: totals, active time and hours") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 30, Cat::Focus, "code");
    minutes(b, at(9, 30), 10, Cat::AiAssistant, "chrome");
    minutes(b, at(10), 20, Cat::Away, "");
    auto s = summarizeDay(b);
    CHECK(s.secondsByCategory[static_cast<std::size_t>(Cat::Focus)] == 1800);
    CHECK(s.secondsByCategory[static_cast<std::size_t>(Cat::Away)] == 1200);
    CHECK(s.activeSeconds == 2400);  // away does not count as active
    CHECK(s.hours[9].seconds[static_cast<std::size_t>(Cat::Focus)] == 1800);
    CHECK(s.hours[10].seconds[static_cast<std::size_t>(Cat::Away)] == 1200);
    CHECK(s.firstActiveMinute == at(9));
    CHECK(s.lastActiveMinute == at(9, 39));
}

TEST_CASE("day: the longest focus block bridges one minute but not two") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 10, Cat::Focus, "code");
    minutes(b, at(9, 10), 1, Cat::Neutral, "chrome");   // a one-minute glance elsewhere
    minutes(b, at(9, 11), 10, Cat::Focus, "code");
    minutes(b, at(9, 21), 2, Cat::Neutral, "slack");    // two minutes away from the work: a real break
    minutes(b, at(9, 23), 30, Cat::Focus, "code");
    auto s = summarizeDay(b);
    CHECK(s.longestFocusMinutes == 30);                 // the last block; the first two were 20 focus minutes together
}

TEST_CASE("day: a glance does not split the block") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 10, Cat::Focus, "code");
    minutes(b, at(9, 10), 1, Cat::Neutral, "chrome");
    minutes(b, at(9, 11), 10, Cat::Focus, "code");
    CHECK(summarizeDay(b).longestFocusMinutes == 20);
}

TEST_CASE("day: context switches count changes of the main app between active minutes") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 5, Cat::Focus, "code");
    minutes(b, at(9, 5), 5, Cat::Neutral, "slack");     // 1 switch
    minutes(b, at(9, 10), 5, Cat::Focus, "code");       // 2 switches
    auto s = summarizeDay(b);
    CHECK(s.contextSwitches == 2);
    CHECK(s.switchesPerActiveHour == doctest::Approx(2.0 / (15.0 * 60.0 / 3600.0)));  // 2 switches in a quarter hour
}

TEST_CASE("day: leaving and coming back to the same app is not a switch") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 5, Cat::Focus, "code");
    minutes(b, at(9, 5), 30, Cat::Away, "");
    minutes(b, at(9, 35), 5, Cat::Focus, "code");
    CHECK(summarizeDay(b).contextSwitches == 0);
}

TEST_CASE("day: top apps are merged, ordered and capped") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 10, Cat::Focus, "code");
    minutes(b, at(9, 10), 3, Cat::Focus, "code");       // same app, same category: merged
    minutes(b, at(9, 13), 20, Cat::AiAssistant, "chrome");
    minutes(b, at(9, 33), 5, Cat::Distraction, "chrome");  // same app, different category: its own row
    minutes(b, at(9, 38), 60, Cat::Away, "");
    auto s = summarizeDay(b, 2);
    REQUIRE(s.topApps.size() == 2);
    CHECK(s.topApps[0].app == "chrome");
    CHECK(s.topApps[0].category == Cat::AiAssistant);
    CHECK(s.topApps[0].seconds == 1200);
    CHECK(s.topApps[1].app == "code");
    CHECK(s.topApps[1].seconds == 780);
}

// ---------------------------------------------------------------------------------- engagement

namespace {
ActivityBucket slice(int minute, Cat c, const char* app, int seconds, int active, int switches = 0, int scroll = 0, int keys = 0,
                     int clicks = 0) {
    ActivityBucket b{"2026-11-23", minute, c, c == Cat::Away ? "" : app, seconds};
    b.activeSeconds = active;
    b.switches      = switches;
    b.scrollEvents  = scroll;
    b.keyEvents     = keys;
    b.clickEvents   = clicks;
    return b;
}
}  // namespace

TEST_CASE("day: engaged time is totalled per category, and Away is left out") {
    std::vector<ActivityBucket> b = {
        slice(at(9), Cat::Focus, "code", 60, 50),
        slice(at(9, 1), Cat::Focus, "code", 60, 10),
        slice(at(9, 2), Cat::Distraction, "chrome", 60, 5),
        slice(at(9, 3), Cat::Away, "", 60, 0),
    };
    auto s = summarizeDay(b);
    CHECK(s.engagedSecondsByCategory[static_cast<std::size_t>(Cat::Focus)] == 60);
    CHECK(s.engagedSecondsByCategory[static_cast<std::size_t>(Cat::Distraction)] == 5);
    CHECK(s.engagedSeconds == 65);
    CHECK(s.engagedShare(Cat::Focus) == doctest::Approx(0.5));            // 60 of 120 s
    CHECK(s.engagedShare(Cat::Distraction) == doctest::Approx(5.0 / 60.0));
    CHECK(s.engagedShare(Cat::Away) == 0.0);
    CHECK(s.engagedShare(Cat::AiAssistant) == 0.0);                        // no time in it: 0, not a division by zero
}

TEST_CASE("day: foreground changes and input counts are summed") {
    std::vector<ActivityBucket> b = {
        slice(at(9), Cat::Focus, "code", 60, 40, 2, 5, 100, 4),
        slice(at(9, 1), Cat::Distraction, "chrome", 60, 30, 9, 80, 0, 12),
    };
    auto s = summarizeDay(b);
    CHECK(s.foregroundChanges == 11);
    CHECK(s.scrollEvents == 85);
    CHECK(s.keyEvents == 100);
    CHECK(s.clickEvents == 16);
}

TEST_CASE("day: a day recorded before engagement existed says it has no engagement data") {
    std::vector<ActivityBucket> b;
    minutes(b, at(9), 30, Cat::Focus, "code");   // the old helper: no engagement fields set
    auto s = summarizeDay(b);
    CHECK_FALSE(s.hasEngagementData);
    CHECK(s.engagedSeconds == 0);                 // unknown, and the flag is how a screen tells that from "idle"
}

TEST_CASE("day: any engagement number at all means the day has engagement data") {
    CHECK(summarizeDay({slice(at(9), Cat::Focus, "code", 60, 20)}).hasEngagementData);
    CHECK(summarizeDay({slice(at(9), Cat::Focus, "code", 60, 0, 3)}).hasEngagementData);
    CHECK(summarizeDay({slice(at(9), Cat::Focus, "code", 60, 0, 0, 0, 0, 1)}).hasEngagementData);
}

TEST_CASE("day: a corrupt engaged value cannot exceed the time it belongs to") {
    auto s = summarizeDay({slice(at(9), Cat::Focus, "code", 30, 500)});
    CHECK(s.engagedSeconds == 30);
    CHECK(s.engagedShare(Cat::Focus) == doctest::Approx(1.0));
}
