#include <doctest/doctest.h>

#include "core/Wellness.h"

using namespace focus;

namespace {
constexpr int at(int h, int m = 0) { return h * 60 + m; }
const std::map<std::string, MealStatus> noMeals;

LocalTime time(int minute, const char* day = "2026-11-23") { return LocalTime{day, minute}; }

std::vector<std::string> keys(const std::vector<WellnessReminder>& r) {
    std::vector<std::string> k;
    for (const auto& x : r) k.push_back(x.key);
    return k;
}
}  // namespace

TEST_CASE("expected glasses follow a straight line across the waking day") {
    const WaterRule rule;  // 8 glasses, 08:00 to 22:00
    CHECK(expectedGlasses(rule, at(7)) == 0);
    CHECK(expectedGlasses(rule, at(8)) == 0);
    CHECK(expectedGlasses(rule, at(15)) == 4);   // halfway through the day, half the goal
    CHECK(expectedGlasses(rule, at(21, 59)) == 7);
    CHECK(expectedGlasses(rule, at(22)) == 8);
    CHECK(expectedGlasses(rule, at(23)) == 8);
}

TEST_CASE("clock text pads with zeros") {
    CHECK(clockText(at(7, 5)) == "07:05");
    CHECK(clockText(at(12, 30)) == "12:30");
    CHECK(clockText(0) == "00:00");
}

TEST_CASE("water: quiet until you are actually behind, then one nudge per gap") {
    WellnessEngine e({}, {});
    WaterFacts none;
    CHECK(e.due(time(at(9, 0)), none, noMeals, true).empty());       // expected 0: nothing to be behind on
    CHECK(e.due(time(at(10, 0)), none, noMeals, true).size() == 1);  // expected 1, logged 0
    CHECK(e.due(time(at(10, 20)), none, noMeals, true).empty());     // too soon after the last nudge
    CHECK(e.due(time(at(10, 50)), none, noMeals, true).size() == 1); // 50 minutes later: nudge again
}

TEST_CASE("water: logging a glass keeps it quiet, and reaching the goal ends it") {
    WellnessEngine e({}, {});
    WaterFacts w;
    w.glasses = 1; w.lastGlassMinute = at(14, 40);
    CHECK(e.due(time(at(15, 0)), w, noMeals, true).empty());         // behind pace (4) but just had a glass

    w.glasses = 1; w.lastGlassMinute = at(10, 0);
    CHECK(e.due(time(at(15, 0)), w, noMeals, true).size() == 1);     // long ago, and far behind

    w.glasses = 8;
    CHECK(e.due(time(at(20, 0)), w, noMeals, true).empty());         // goal reached
}

TEST_CASE("water: the message says how far behind you are") {
    WellnessEngine e({}, {});
    WaterFacts w; w.glasses = 1; w.lastGlassMinute = at(8, 30);
    auto r = e.due(time(at(15, 0)), w, noMeals, true);
    REQUIRE(r.size() == 1);
    CHECK(r[0].body.find("1 of 8") != std::string::npos);
    CHECK(r[0].body.find("3 glasses behind") != std::string::npos);
}

TEST_CASE("nothing outside waking hours") {
    WellnessEngine e({}, {});
    WaterFacts w;
    CHECK(e.due(time(at(6, 0)), w, noMeals, true).empty());
    CHECK(e.due(time(at(22, 30)), w, noMeals, true).empty());
}

TEST_CASE("an away user is not reminded, and the reminder is still due when they return") {
    WellnessEngine e({}, {});
    WaterFacts w;
    CHECK(e.due(time(at(11, 0)), w, noMeals, false).empty());        // away: held back
    CHECK(e.due(time(at(11, 5)), w, noMeals, true).size() == 1);     // back: not "used up"
}

TEST_CASE("meals: due when the window opens, repeated while unlogged, silent once handled") {
    WellnessEngine e({}, defaultMeals());
    WaterFacts w; w.glasses = 8;  // keep water out of this test
    CHECK(e.due(time(at(12, 0)), w, noMeals, true).empty());                  // lunch not open yet
    CHECK(keys(e.due(time(at(12, 30)), w, noMeals, true)) == std::vector<std::string>{"meal:lunch"});
    CHECK(e.due(time(at(12, 45)), w, noMeals, true).empty());                 // repeat is every 30 min
    CHECK(keys(e.due(time(at(13, 0)), w, noMeals, true)) == std::vector<std::string>{"meal:lunch"});

    const std::map<std::string, MealStatus> ate = {{"lunch", MealStatus::Ate}};
    CHECK(e.due(time(at(13, 30)), w, ate, true).empty());
    const std::map<std::string, MealStatus> skipped = {{"lunch", MealStatus::Skipped}};
    CHECK(e.due(time(at(14, 0)), w, skipped, true).empty());
}

TEST_CASE("meals: a missed window stops reminding") {
    WellnessEngine e({}, defaultMeals());
    WaterFacts w; w.glasses = 8;
    CHECK(e.due(time(at(14, 31)), w, noMeals, true).empty());  // lunch ended at 14:30
}

TEST_CASE("meal states over the day") {
    const auto lunch = defaultMeals()[1];
    CHECK(mealState(lunch, std::nullopt, at(11)) == MealState::Upcoming);
    CHECK(mealState(lunch, std::nullopt, at(13)) == MealState::Due);
    CHECK(mealState(lunch, std::nullopt, at(15)) == MealState::Missed);
    CHECK(mealState(lunch, MealStatus::Ate, at(13)) == MealState::Ate);
    CHECK(mealState(lunch, MealStatus::Skipped, at(15)) == MealState::Skipped);
}

TEST_CASE("a new day starts with a clean slate") {
    WellnessEngine e({}, {});
    WaterFacts w;
    CHECK(e.due(time(at(10, 0), "2026-11-23"), w, noMeals, true).size() == 1);
    CHECK(e.due(time(at(10, 5), "2026-11-23"), w, noMeals, true).empty());
    CHECK(e.due(time(at(10, 5), "2026-11-24"), w, noMeals, true).size() == 1);  // next day: nudged again right away
}

TEST_CASE("the goal can be changed") {
    WellnessEngine e({}, {});
    e.setWaterGoal(12);
    CHECK(e.waterRule().goalGlasses == 12);
    e.setWaterGoal(99);
    CHECK(e.waterRule().goalGlasses == 20);  // clamped
    e.setWaterGoal(0);
    CHECK(e.waterRule().goalGlasses == 1);
}
