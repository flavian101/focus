#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "core/Activity.h"
#include "core/Wellness.h"  // LocalTime

namespace focus {

// A slice of one minute of the day spent in one category and app. This is all that is ever kept:
// no window titles, no keystrokes.
struct ActivityBucket {
    std::string day;       // local date "YYYY-MM-DD"
    int minuteOfDay = 0;   // 0..1439
    ActivityCategory category = ActivityCategory::Neutral;
    std::string app;       // "code", "chrome", ... ("" for Away)
    int seconds = 0;

    // Engagement: how the person was using the keyboard and mouse during these seconds. Only numbers.
    // Which keys, what was typed and where the pointer was are never looked at, let alone kept.
    int activeSeconds = 0;  // seconds with keyboard/mouse input (never more than `seconds`)
    int switches      = 0;  // times the foreground window or its title changed
    int scrollEvents  = 0;  // wheel ticks; only counted in Detailed mode
    int keyEvents     = 0;  // key presses; only counted in Detailed mode
    int clickEvents   = 0;  // mouse button presses; only counted in Detailed mode

    // Adds another slice of the same minute, category and app into this one.
    void accumulate(const ActivityBucket& other) {
        seconds += other.seconds;
        activeSeconds += other.activeSeconds;
        switches += other.switches;
        scrollEvents += other.scrollEvents;
        keyEvents += other.keyEvents;
        clickEvents += other.clickEvents;
    }
};

// Turns "what was in front at this moment" samples into per-minute buckets.
//
//  * Sample and hold: the time since the previous sample is credited to the PREVIOUS state.
//  * A gap longer than maxGap (laptop asleep, screen locked, app frozen) is not credited to anything.
//  * Buckets are held in memory for a few minutes before being handed out. That is what lets
//    recordAway() correct the past: when idle time crosses the "away" threshold, the whole idle
//    stretch is moved from whatever app was credited to Away.
//
// Like every class here it is handed "now"; it never reads a clock.
class ActivityTracker {
public:
    using Clock = std::chrono::steady_clock;

    explicit ActivityTracker(std::chrono::seconds maxGap = std::chrono::seconds(15)) : maxGap_(maxGap) {}

    void record(Clock::time_point now, const LocalTime& local, ActivityCategory category, const std::string& app);
    // The user has had no input for idleFor. Credits Away, and on the first such sample of an
    // absence moves that idle stretch out of the buckets that were wrongly crediting an app.
    void recordAway(Clock::time_point now, const LocalTime& local, std::chrono::seconds idleFor);

    // Hand out buckets that are old enough to be final: any from an earlier day, and from today
    // those at least `holdBackMinutes` behind the current minute.
    std::vector<ActivityBucket> takeFlushable(const LocalTime& local, int holdBackMinutes);
    std::vector<ActivityBucket> takeAll();
    std::vector<ActivityBucket> pending() const { return buckets_; }  // a copy, for merging into reports

    // Re-label buffered time for one app: "this app is really <to>". With onlyFrom set, only time
    // currently in that category moves (used for browsers, so AI and distraction tabs are left alone).
    void recategorize(const std::string& day, const std::string& app, std::optional<ActivityCategory> onlyFrom, ActivityCategory to);

    void discardDay(const std::string& day);
    void forget();  // drop the "previous sample" (tracking was paused) so the pause is not credited

private:
    struct Sample {
        Clock::time_point time;
        LocalTime local;
        ActivityCategory category;
        std::string app;
    };

    void creditPrevious(Clock::time_point now);
    void add(const std::string& day, int minute, ActivityCategory category, const std::string& app, int seconds);

    std::chrono::seconds maxGap_;
    std::optional<Sample> prev_;
    std::vector<ActivityBucket> buckets_;  // in the order they first appeared, so roughly oldest first
};

}  // namespace focus
