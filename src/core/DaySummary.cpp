#include "core/DaySummary.h"

#include <algorithm>
#include <map>

namespace focus {

namespace {
constexpr int kFocusMinuteSeconds = 30;
constexpr int kBridgeMinutes      = 1;  // one non-focus minute does not end a focus block
constexpr std::size_t idx(ActivityCategory c) { return static_cast<std::size_t>(c); }
}  // namespace

DaySummary summarizeDay(const std::vector<ActivityBucket>& buckets, std::size_t maxApps) {
    DaySummary s;
    for (int h = 0; h < 24; ++h) s.hours[static_cast<std::size_t>(h)].hour = h;

    struct Minute {
        int focusSeconds = 0;
        std::map<std::string, int> activeByApp;  // non-away seconds per app
    };
    std::map<int, Minute> minutes;
    std::map<std::pair<std::string, ActivityCategory>, int> apps;

    for (const auto& b : buckets) {
        if (b.seconds <= 0 || b.minuteOfDay < 0 || b.minuteOfDay >= 24 * 60) continue;
        s.secondsByCategory[idx(b.category)] += b.seconds;
        s.hours[static_cast<std::size_t>(b.minuteOfDay / 60)].seconds[idx(b.category)] += b.seconds;
        if (b.category == ActivityCategory::Away) continue;

        s.activeSeconds += b.seconds;
        const int engaged = std::clamp(b.activeSeconds, 0, b.seconds);
        s.engagedSecondsByCategory[idx(b.category)] += engaged;
        s.engagedSeconds += engaged;
        s.foregroundChanges += std::max(b.switches, 0);
        s.scrollEvents += std::max(b.scrollEvents, 0);
        s.keyEvents += std::max(b.keyEvents, 0);
        s.clickEvents += std::max(b.clickEvents, 0);
        if (engaged > 0 || b.switches > 0 || b.scrollEvents > 0 || b.keyEvents > 0 || b.clickEvents > 0) s.hasEngagementData = true;
        auto& m = minutes[b.minuteOfDay];
        m.activeByApp[b.app] += b.seconds;
        if (b.category == ActivityCategory::Focus) m.focusSeconds += b.seconds;
        apps[{b.app, b.category}] += b.seconds;
    }

    // first/last activity, focus blocks and context switches all walk the minutes in order
    int previousFocusMinute = -1000, run = 0;
    std::string previousApp;
    bool havePreviousApp = false;
    for (const auto& [minute, m] : minutes) {
        if (s.firstActiveMinute < 0) s.firstActiveMinute = minute;
        s.lastActiveMinute = minute;

        if (m.focusSeconds >= kFocusMinuteSeconds) {
            run = (minute - previousFocusMinute <= kBridgeMinutes + 1) ? run + 1 : 1;
            previousFocusMinute = minute;
            s.longestFocusMinutes = std::max(s.longestFocusMinutes, run);
        }

        // the app that had most of this minute (ties: alphabetical, so the result is deterministic)
        const auto dominant = std::max_element(m.activeByApp.begin(), m.activeByApp.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second < b.second : a.first > b.first;
        });
        if (havePreviousApp && dominant->first != previousApp) ++s.contextSwitches;
        previousApp     = dominant->first;
        havePreviousApp = true;
    }
    if (s.activeSeconds > 0) s.switchesPerActiveHour = s.contextSwitches / (s.activeSeconds / 3600.0);

    for (const auto& [key, seconds] : apps) s.topApps.push_back({key.first, key.second, seconds});
    std::sort(s.topApps.begin(), s.topApps.end(), [](const AppUse& a, const AppUse& b) {
        return a.seconds != b.seconds ? a.seconds > b.seconds : a.app < b.app;
    });
    if (s.topApps.size() > maxApps) s.topApps.resize(maxApps);
    return s;
}

}  // namespace focus
