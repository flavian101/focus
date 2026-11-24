#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ActivityTracker.h"

namespace focus {

struct HourSummary {
    int hour = 0;  // 0..23
    std::array<int, kActivityCategoryCount> seconds{};  // indexed by ActivityCategory
};

struct AppUse {
    std::string app;
    ActivityCategory category = ActivityCategory::Neutral;
    int seconds = 0;
};

// What a day looked like. Everything is computed from the buckets, so it can be tested with made-up data.
struct DaySummary {
    std::array<std::int64_t, kActivityCategoryCount> secondsByCategory{};
    int activeSeconds = 0;       // everything except Away
    int firstActiveMinute = -1;  // minute of day, -1 = no activity
    int lastActiveMinute  = -1;

    // A "focus minute" has at least 30 s of Focus. A focus block is a run of focus minutes where
    // a single non-focus minute in between (a quick glance elsewhere) does not break it.
    int longestFocusMinutes = 0;

    // How often the main app changed between active minutes. Fewer switches per hour = deeper work.
    int contextSwitches = 0;
    double switchesPerActiveHour = 0.0;

    std::array<HourSummary, 24> hours;
    std::vector<AppUse> topApps;  // most used first, per (app, category)
};

DaySummary summarizeDay(const std::vector<ActivityBucket>& buckets, std::size_t maxApps = 8);

}  // namespace focus
