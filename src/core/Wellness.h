#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace focus {

// The caller's local clock, as plain values. Core never asks the OS for the time or a time zone.
struct LocalTime {
    std::string day;       // "YYYY-MM-DD"
    int minuteOfDay = 0;   // 0 = midnight, 12:30 = 750
};

// "Drink enough water" as a pace, not a timer: over your waking day you aim for `goalGlasses`,
// and you are reminded only when you have fallen behind that pace.
struct WaterRule {
    int goalGlasses            = 8;
    int dayStartMinute         = 8 * 60;
    int dayEndMinute           = 22 * 60;
    int reminderGapMinutes     = 45;  // never nag more often than this
    int quietAfterGlassMinutes = 30;  // after you log a glass, stay quiet this long
};

struct MealWindow {
    std::string id;    // stable key, e.g. "lunch"
    std::string name;  // e.g. "Lunch"
    int startMinute = 0;
    int endMinute   = 0;
    int repeatMinutes = 30;  // while the window is open and the meal is not logged
};

std::vector<MealWindow> defaultMeals();

enum class MealStatus { Ate, Skipped };                               // what you told us
enum class MealState { Upcoming, Due, Ate, Skipped, Missed };         // where the meal stands right now

struct WaterFacts {
    int glasses         = 0;   // logged so far today
    int lastGlassMinute = -1;  // minute of day of the latest glass, -1 = none yet
};

struct WellnessReminder {
    std::string key;    // "water" or "meal:<id>"
    std::string title;
    std::string body;
};

// Where you should be by `minuteOfDay` to reach the goal by the end of the day: 0 before the day
// starts, the full goal after it ends, and a straight line in between (rounded down).
int expectedGlasses(const WaterRule& rule, int minuteOfDay);

MealState mealState(const MealWindow& meal, const std::optional<MealStatus>& logged, int minuteOfDay);

std::string clockText(int minuteOfDay);  // 750 -> "12:30"

// Decides what to say, and when. It remembers when it last spoke (so it does not nag), but
// everything else is passed in, so it is easy to test with made-up times.
class WellnessEngine {
public:
    WellnessEngine(WaterRule water, std::vector<MealWindow> meals);

    const WaterRule& waterRule() const { return water_; }
    const std::vector<MealWindow>& meals() const { return meals_; }
    void setWaterGoal(int glasses);

    // `present` = the user is at the keyboard. A reminder for someone who is away is held back
    // (not "used up"), so it is shown as soon as they return.
    std::vector<WellnessReminder> due(const LocalTime& now, const WaterFacts& water,
                                      const std::map<std::string, MealStatus>& meals, bool present);

private:
    bool gapPassed(const std::string& key, int minute, int gap) const;

    WaterRule water_;
    std::vector<MealWindow> meals_;
    std::string day_;
    std::map<std::string, int> lastReminded_;  // key -> minute of day
};

}  // namespace focus
