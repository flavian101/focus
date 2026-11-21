#include <doctest/doctest.h>

#include "core/Timer.h"

using namespace std::chrono_literals;
using focus::Timer;

namespace {
const Timer::Clock::time_point t0{};  // an arbitrary fixed starting instant
}

TEST_CASE("Timer counts down while running") {
    Timer t;
    t.start(25min, t0);
    CHECK(t.isRunning());
    CHECK(t.remaining(t0 + 10min) == 15min);
    CHECK_FALSE(t.expired(t0 + 24min));
    CHECK(t.expired(t0 + 25min));
}

TEST_CASE("Timer does not count paused time") {
    Timer t;
    t.start(10min, t0);
    t.pause(t0 + 4min);
    CHECK(t.isPaused());
    CHECK(t.remaining(t0 + 60min) == 6min);  // an hour later, still 6 minutes left
    t.resume(t0 + 60min);
    CHECK(t.remaining(t0 + 62min) == 4min);
    CHECK(t.expired(t0 + 66min));
}

TEST_CASE("Timer ignores invalid transitions") {
    Timer t;
    t.pause(t0);   // nothing running: no-op
    t.resume(t0);  // nothing paused: no-op
    CHECK_FALSE(t.isActive());
    CHECK(t.remaining(t0) == 0s);
    CHECK_FALSE(t.expired(t0));
}

TEST_CASE("Timer stop resets it") {
    Timer t;
    t.start(5min, t0);
    t.stop();
    CHECK_FALSE(t.isActive());
    CHECK(t.elapsed(t0 + 1min) == 0s);
}
