#include "core/Timer.h"

#include <algorithm>

namespace focus {

void Timer::start(Seconds duration, Clock::time_point now) {
    duration_    = duration;
    accumulated_ = Clock::duration::zero();
    runStart_    = now;
    state_       = State::Running;
}

void Timer::pause(Clock::time_point now) {
    if (state_ != State::Running) return;
    accumulated_ += now - runStart_;  // bank the time of this segment
    state_ = State::Paused;
}

void Timer::resume(Clock::time_point now) {
    if (state_ != State::Paused) return;
    runStart_ = now;  // a new segment starts; paused time is never counted
    state_    = State::Running;
}

void Timer::stop() {
    state_       = State::Stopped;
    accumulated_ = Clock::duration::zero();
}

Timer::Clock::duration Timer::elapsedPrecise(Clock::time_point now) const {
    if (state_ == State::Stopped) return Clock::duration::zero();
    auto total = accumulated_;
    if (state_ == State::Running) total += now - runStart_;
    return std::max(total, Clock::duration::zero());
}

Timer::Seconds Timer::elapsed(Clock::time_point now) const {
    return std::chrono::duration_cast<Seconds>(elapsedPrecise(now));
}

Timer::Seconds Timer::remaining(Clock::time_point now) const {
    if (state_ == State::Stopped) return Seconds::zero();
    return std::max(duration_ - elapsed(now), Seconds::zero());
}

bool Timer::expired(Clock::time_point now) const {
    return state_ != State::Stopped && elapsedPrecise(now) >= duration_;
}

}  // namespace focus
