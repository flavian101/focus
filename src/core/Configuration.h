#pragma once
#include <chrono>
#include <vector>

#include "core/Wellness.h"

namespace focus {

// Tunable settings with sane defaults.
// v1 uses the defaults in code. Loading/saving user overrides arrives with
// Storage/Preferences, so nothing here needs to change when that lands.
struct Configuration {
    std::chrono::minutes focusDuration{25};
    int defaultAiBudget = 3;                         // AI visits allowed per session unless overridden
    std::chrono::seconds activitySampleInterval{2};  // how often we look at the foreground window
    std::chrono::seconds awayThreshold{300};         // no input for this long = you are away
    int maxMustPerDay = 3;                           // a day plan may hold at most this many "Must" tasks
    std::chrono::minutes dailyFocusCapacity{240};    // realistic focused work per day, for over-commit warnings
    WaterRule water;                                 // goal and waking hours for the water pace
    std::vector<MealWindow> meals = defaultMeals();  // when to expect breakfast, lunch and dinner
    std::chrono::seconds ambientSampleInterval{5};   // how often all-day tracking looks at the foreground window
    std::chrono::seconds activityFlushInterval{60}; // how often finished minutes are saved to the database
    bool wellnessEnabled  = true;                    // water and meal reminders, all day while the app runs
    bool remindersEnabled = true;                    // hydration / stretch / eye-care nudges during a session

    static Configuration defaults() { return {}; }
};

}  // namespace focus
