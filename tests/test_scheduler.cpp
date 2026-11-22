#include <doctest/doctest.h>

#include "core/Scheduler.h"
#include "services/Reminders.h"

#include <set>

using namespace std::chrono_literals;
using focus::Reminder;
using focus::Scheduler;

namespace {
const Scheduler::Clock::time_point t0{};

Reminder every(const char* id, std::chrono::seconds interval) { return {id, id, "msg", interval}; }
}  // namespace

TEST_CASE("a reminder fires once its interval has passed, then again one interval later") {
    Scheduler s;
    s.add(every("water", 60min));
    s.start(t0);
    CHECK(s.collectDue(t0 + 59min).empty());
    REQUIRE(s.collectDue(t0 + 60min).size() == 1);
    CHECK(s.collectDue(t0 + 61min).empty());  // already handled
    CHECK(s.collectDue(t0 + 120min).size() == 1);
}

TEST_CASE("a long stall gives one reminder, not a burst of catch-up reminders") {
    Scheduler s;
    s.add(every("eyes", 20min));
    s.start(t0);
    CHECK(s.collectDue(t0 + 5h).size() == 1);   // five hours later: one reminder
    CHECK(s.collectDue(t0 + 5h + 1min).empty());
}

TEST_CASE("reminders are independent") {
    Scheduler s;
    s.add(every("eyes", 20min));
    s.add(every("stretch", 45min));
    s.start(t0);
    auto at20 = s.collectDue(t0 + 20min);
    REQUIRE(at20.size() == 1);
    CHECK(at20[0].id == "eyes");
    // Eyes re-armed at 20min, so it is next due at 40min; stretch is due at 45min.
    auto at40 = s.collectDue(t0 + 40min);
    REQUIRE(at40.size() == 1);
    CHECK(at40[0].id == "eyes");
    auto at45 = s.collectDue(t0 + 45min);
    REQUIRE(at45.size() == 1);
    CHECK(at45[0].id == "stretch");
}

TEST_CASE("paused time does not count toward an interval") {
    Scheduler s;
    s.add(every("water", 10min));
    s.start(t0);
    s.pause(t0 + 4min);
    CHECK(s.collectDue(t0 + 100min).empty());  // nothing fires while paused
    s.resume(t0 + 100min);                     // 6 minutes of the interval were still left
    CHECK(s.collectDue(t0 + 105min).empty());
    CHECK(s.collectDue(t0 + 106min).size() == 1);
}

TEST_CASE("nothing fires before start() or after stop()") {
    Scheduler s;
    s.add(every("water", 1min));
    CHECK(s.collectDue(t0 + 10min).empty());
    s.start(t0);
    s.stop();
    CHECK(s.collectDue(t0 + 10min).empty());
    CHECK_FALSE(s.isRunning());
}

TEST_CASE("a reminder needs a positive interval") {
    Scheduler s;
    CHECK_FALSE(s.add(every("bad", 0s)));
    CHECK_FALSE(s.add(every("worse", -5s)));
    CHECK(s.add(every("ok", 1s)));
}

TEST_CASE("the built-in reminders are valid and have unique ids") {
    const auto reminders = focus::defaultReminders();
    CHECK(reminders.size() == 3);
    std::set<std::string> ids;
    for (const auto& r : reminders) {
        CHECK(r.interval > 0s);
        CHECK_FALSE(r.title.empty());
        ids.insert(r.id);
    }
    CHECK(ids.size() == reminders.size());
}
