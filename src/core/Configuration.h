#pragma once
#include <chrono>

namespace focus {

// Tunable settings with sane defaults.
// v1 uses the defaults in code. Loading/saving user overrides arrives with
// Storage/Preferences, so nothing here needs to change when that lands.
struct Configuration {
    std::chrono::minutes focusDuration{25};
    int defaultAiBudget = 3;                         // AI visits allowed per session unless overridden
    std::chrono::seconds activitySampleInterval{2};  // how often we look at the foreground window
    std::chrono::seconds awayThreshold{300};         // no input for this long = you are away
    bool remindersEnabled = true;                    // hydration / stretch / eye-care nudges during a session

    static Configuration defaults() { return {}; }
};

}  // namespace focus
