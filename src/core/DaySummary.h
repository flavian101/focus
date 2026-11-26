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

    // Engagement: how much of the time had real keyboard/mouse input, and how restless the foreground was.
    // Away is left out (it never has input by definition). All of this is numbers, never content.
    std::array<std::int64_t, kActivityCategoryCount> engagedSecondsByCategory{};  // seconds with input, per category
    int engagedSeconds    = 0;  // across every category except Away
    int foregroundChanges = 0;  // how often the foreground window or its title changed (not the same as contextSwitches)
    int scrollEvents      = 0;  // Detailed mode only
    int keyEvents         = 0;  // Detailed mode only
    int clickEvents       = 0;  // Detailed mode only

    // False for a day recorded before engagement existed (or with tracking that never had a chance to see any),
    // so a screen can say "unknown" instead of claiming 0% engaged.
    bool hasEngagementData = false;

    // Share (0..1) of a category's time that had input. 0 when there is no time in it.
    double engagedShare(ActivityCategory c) const {
        const auto total = secondsByCategory[static_cast<std::size_t>(c)];
        return total > 0 ? static_cast<double>(engagedSecondsByCategory[static_cast<std::size_t>(c)]) / static_cast<double>(total) : 0.0;
    }

    std::array<HourSummary, 24> hours;
    std::vector<AppUse> topApps;  // most used first, per (app, category)
};

DaySummary summarizeDay(const std::vector<ActivityBucket>& buckets, std::size_t maxApps = 8);

}  // namespace focus
