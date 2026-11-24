#include "core/Wellness.h"

#include <algorithm>

namespace focus {

std::vector<MealWindow> defaultMeals() {
    return {
        {"breakfast", "Breakfast", 7 * 60 + 30, 10 * 60, 30},
        {"lunch", "Lunch", 12 * 60 + 30, 14 * 60 + 30, 30},
        {"dinner", "Dinner", 18 * 60 + 30, 21 * 60, 30},
    };
}

std::string clockText(int minuteOfDay) {
    const int m = std::clamp(minuteOfDay, 0, 24 * 60 - 1);
    const std::string hh = std::to_string(m / 60), mm = std::to_string(m % 60);
    return (hh.size() < 2 ? "0" : "") + hh + ":" + (mm.size() < 2 ? "0" : "") + mm;
}

int expectedGlasses(const WaterRule& rule, int minuteOfDay) {
    if (minuteOfDay <= rule.dayStartMinute) return 0;
    if (minuteOfDay >= rule.dayEndMinute) return rule.goalGlasses;
    const int span = rule.dayEndMinute - rule.dayStartMinute;
    return rule.goalGlasses * (minuteOfDay - rule.dayStartMinute) / span;  // integer division rounds down
}

MealState mealState(const MealWindow& meal, const std::optional<MealStatus>& logged, int minuteOfDay) {
    if (logged) return *logged == MealStatus::Ate ? MealState::Ate : MealState::Skipped;
    if (minuteOfDay < meal.startMinute) return MealState::Upcoming;
    if (minuteOfDay < meal.endMinute) return MealState::Due;
    return MealState::Missed;
}

WellnessEngine::WellnessEngine(WaterRule water, std::vector<MealWindow> meals)
    : water_(water), meals_(std::move(meals)) {}

void WellnessEngine::setWaterGoal(int glasses) { water_.goalGlasses = std::clamp(glasses, 1, 20); }

bool WellnessEngine::gapPassed(const std::string& key, int minute, int gap) const {
    const auto it = lastReminded_.find(key);
    return it == lastReminded_.end() || minute - it->second >= gap;
}

std::vector<WellnessReminder> WellnessEngine::due(const LocalTime& now, const WaterFacts& water,
                                                  const std::map<std::string, MealStatus>& meals, bool present) {
    if (now.day != day_) {  // a new day starts with a clean slate
        day_ = now.day;
        lastReminded_.clear();
    }
    std::vector<WellnessReminder> out;
    if (!present) return out;  // held back, not used up: it will still be due when they return

    // ---- water ---------------------------------------------------------------------------
    const bool inDay    = now.minuteOfDay >= water_.dayStartMinute && now.minuteOfDay < water_.dayEndMinute;
    const int expected  = expectedGlasses(water_, now.minuteOfDay);
    const bool behind   = water.glasses < expected && water.glasses < water_.goalGlasses;
    const bool quiet    = water.lastGlassMinute >= 0 && now.minuteOfDay - water.lastGlassMinute < water_.quietAfterGlassMinutes;
    if (inDay && behind && !quiet && gapPassed("water", now.minuteOfDay, water_.reminderGapMinutes)) {
        const int gap = expected - water.glasses;
        std::string body = water.glasses == 0
            ? "No water logged yet today. A glass now gets you started."
            : "You're at " + std::to_string(water.glasses) + " of " + std::to_string(water_.goalGlasses) +
              " glasses, " + std::to_string(gap) + (gap == 1 ? " glass" : " glasses") + " behind pace. Have one now.";
        out.push_back({"water", "Time for water", std::move(body)});
        lastReminded_["water"] = now.minuteOfDay;
    }

    // ---- meals -------------------------------------------------------------------------------
    for (const auto& meal : meals_) {
        std::optional<MealStatus> logged;
        if (const auto it = meals.find(meal.id); it != meals.end()) logged = it->second;
        if (mealState(meal, logged, now.minuteOfDay) != MealState::Due) continue;

        const std::string key = "meal:" + meal.id;
        if (!gapPassed(key, now.minuteOfDay, meal.repeatMinutes)) continue;
        out.push_back({key, meal.name + " time",
                       "Your " + meal.name + " window is " + clockText(meal.startMinute) + " to " + clockText(meal.endMinute) +
                           ". Eat something, then mark it done."});
        lastReminded_[key] = now.minuteOfDay;
    }
    return out;
}

}  // namespace focus
