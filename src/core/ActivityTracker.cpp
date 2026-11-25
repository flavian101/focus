#include "core/ActivityTracker.h"

#include <algorithm>
#include <cmath>

namespace focus {

void ActivityTracker::add(const std::string& day, int minute, ActivityCategory category, const std::string& app, int seconds) {
    if (seconds <= 0) return;
    // Search from the back: the bucket we want is almost always one of the most recent.
    for (auto it = buckets_.rbegin(); it != buckets_.rend(); ++it) {
        if (it->minuteOfDay == minute && it->category == category && it->day == day && it->app == app) {
            it->seconds += seconds;
            return;
        }
    }
    buckets_.push_back({day, minute, category, app, seconds});
}

void ActivityTracker::creditPrevious(Clock::time_point now) {
    if (!prev_) return;
    const auto elapsed = std::min<Clock::duration>(now - prev_->time, maxGap_);  // a long gap is not credited
    const int seconds  = static_cast<int>(std::lround(std::chrono::duration<double>(elapsed).count()));
    add(prev_->local.day, prev_->local.minuteOfDay, prev_->category, prev_->app, seconds);
}

void ActivityTracker::record(Clock::time_point now, const LocalTime& local, ActivityCategory category, const std::string& app) {
    creditPrevious(now);
    prev_ = Sample{now, local, category, category == ActivityCategory::Away ? std::string() : app};
}

void ActivityTracker::recordAway(Clock::time_point now, const LocalTime& local, std::chrono::seconds idleFor) {
    const bool alreadyAway = prev_ && prev_->category == ActivityCategory::Away;
    creditPrevious(now);

    if (!alreadyAway) {
        // Without input the foreground cannot have changed, so the idle stretch was credited to
        // whatever buckets are newest. Take it back from them, newest first, and give it to Away.
        struct Move { std::string day; int minute; int seconds; };
        std::vector<Move> moves;
        int remaining = static_cast<int>(idleFor.count());
        for (auto it = buckets_.rbegin(); it != buckets_.rend() && remaining > 0; ++it) {
            if (it->category == ActivityCategory::Away) continue;
            const int take = std::min(remaining, it->seconds);
            it->seconds -= take;
            remaining   -= take;
            moves.push_back({it->day, it->minuteOfDay, take});
        }
        buckets_.erase(std::remove_if(buckets_.begin(), buckets_.end(), [](const ActivityBucket& b) { return b.seconds <= 0; }),
                       buckets_.end());
        for (const auto& m : moves) add(m.day, m.minute, ActivityCategory::Away, std::string(), m.seconds);
    }
    prev_ = Sample{now, local, ActivityCategory::Away, std::string()};
}

std::vector<ActivityBucket> ActivityTracker::takeFlushable(const LocalTime& local, int holdBackMinutes) {
    std::vector<ActivityBucket> out, keep;
    for (auto& b : buckets_) {
        const bool final = b.day != local.day || b.minuteOfDay <= local.minuteOfDay - holdBackMinutes;
        (final ? out : keep).push_back(std::move(b));
    }
    buckets_ = std::move(keep);
    return out;
}

std::vector<ActivityBucket> ActivityTracker::takeAll() {
    std::vector<ActivityBucket> out = std::move(buckets_);
    buckets_.clear();
    return out;
}

void ActivityTracker::recategorize(const std::string& day, const std::string& app, std::optional<ActivityCategory> onlyFrom,
                                   ActivityCategory to) {
    if (to == ActivityCategory::Away) return;  // away is decided by the keyboard, not by the app

    // The held "previous sample" will be credited at the next sample, so it must carry the new label too.
    if (prev_ && prev_->local.day == day && prev_->app == app && prev_->category != to && prev_->category != ActivityCategory::Away &&
        (!onlyFrom || prev_->category == *onlyFrom)) {
        prev_->category = to;
    }

    std::vector<ActivityBucket> moved;
    for (auto& b : buckets_) {
        if (b.day != day || b.app != app || b.category == to || b.category == ActivityCategory::Away) continue;
        if (onlyFrom && b.category != *onlyFrom) continue;
        moved.push_back(b);
        b.seconds = 0;
    }
    buckets_.erase(std::remove_if(buckets_.begin(), buckets_.end(), [](const ActivityBucket& b) { return b.seconds <= 0; }), buckets_.end());
    for (const auto& m : moved) add(m.day, m.minuteOfDay, to, m.app, m.seconds);  // merges into an existing bucket if there is one
}

void ActivityTracker::discardDay(const std::string& day) {
    buckets_.erase(std::remove_if(buckets_.begin(), buckets_.end(), [&](const ActivityBucket& b) { return b.day == day; }),
                   buckets_.end());
}

void ActivityTracker::forget() { prev_.reset(); }

}  // namespace focus
