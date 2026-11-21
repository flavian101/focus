#pragma once
#include <chrono>

namespace focus {

// A countdown that can be paused.
//
// Design note: the Timer never calls Clock::now() itself. The caller passes "now" in.
// That makes it deterministic in unit tests (no sleeping) and keeps it free of any
// OS dependency.
class Timer {
public:
    using Clock   = std::chrono::steady_clock;  // monotonic: never jumps when the user changes system time
    using Seconds = std::chrono::seconds;

    void start(Seconds duration, Clock::time_point now);
    void pause(Clock::time_point now);
    void resume(Clock::time_point now);
    void stop();

    bool isRunning() const { return state_ == State::Running; }
    bool isPaused() const { return state_ == State::Paused; }
    bool isActive() const { return state_ != State::Stopped; }

    Seconds elapsed(Clock::time_point now) const;
    Seconds remaining(Clock::time_point now) const;
    bool expired(Clock::time_point now) const;

private:
    enum class State { Stopped, Running, Paused };

    Clock::duration elapsedPrecise(Clock::time_point now) const;

    State state_ = State::Stopped;
    Seconds duration_{0};
    Clock::duration accumulated_{0};  // time counted before the current run segment
    Clock::time_point runStart_{};    // when the current run segment began
};

}  // namespace focus
