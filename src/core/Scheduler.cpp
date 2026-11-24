#include "core/Scheduler.h"

namespace focus {

bool Scheduler::add(Reminder reminder) {
    if (reminder.interval <= std::chrono::seconds::zero()) return false;
    entries_.push_back(Entry{std::move(reminder), {}});
    return true;
}

void Scheduler::start(Clock::time_point now) {
    for (auto& e : entries_) e.nextDue = now + e.reminder.interval;
    running_ = true;
    paused_  = false;
}

void Scheduler::stop() {
    running_ = false;
    paused_  = false;
}

void Scheduler::pause(Clock::time_point now) {
    if (!running_ || paused_) return;
    paused_   = true;
    pausedAt_ = now;
}

void Scheduler::resume(Clock::time_point now) {
    if (!running_ || !paused_) return;
    const auto pausedFor = now - pausedAt_;
    for (auto& e : entries_) e.nextDue += pausedFor;  // the pause does not count toward any interval
    paused_ = false;
}

std::vector<Reminder> Scheduler::collectDue(Clock::time_point now) {
    std::vector<Reminder> due;
    if (!running_ || paused_) return due;
    for (auto& e : entries_) {
        if (now >= e.nextDue) {
            due.push_back(e.reminder);
            e.nextDue = now + e.reminder.interval;
        }
    }
    return due;
}

}  // namespace focus
