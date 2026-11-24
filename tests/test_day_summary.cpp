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
