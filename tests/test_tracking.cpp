#include <doctest/doctest.h>

#include "core/Tracking.h"

using namespace focus;

namespace {
LocalTime at(const char* day, int h, int m = 0) { return LocalTime{day, h * 60 + m}; }
}  // namespace

TEST_CASE("weekday of a date, Monday = 0") {
    CHECK(weekdayOfIso("2026-11-23") == 0);  // a Monday
    CHECK(weekdayOfIso("2026-11-25") == 2);  // Wednesday
    CHECK(weekdayOfIso("2026-11-29") == 6);  // Sunday
    CHECK(weekdayOfIso("1970-01-01") == 3);  // Thursday
    CHECK(weekdayOfIso("2000-01-01") == 5);  // Saturday
    CHECK(weekdayOfIso("2024-02-29") == 3);  // a leap day, Thursday
    CHECK(weekdayOfIso("2024-03-01") == 4);  // the day after it, Friday
}

TEST_CASE("weekday rejects things that are not dates") {
    CHECK(weekdayOfIso("") == -1);
    CHECK(weekdayOfIso("2026/11/23") == -1);
    CHECK(weekdayOfIso("2026-13-01") == -1);
    CHECK(weekdayOfIso("2026-11-xx") == -1);
}

TEST_CASE("work hours: weekdays 09:00 to 17:00 by default, end exclusive") {
    const WorkSchedule s;
    CHECK_FALSE(s.contains(at("2026-11-25", 8, 59)));
    CHECK(s.contains(at("2026-11-25", 9, 0)));
    CHECK(s.contains(at("2026-11-25", 16, 59)));
    CHECK_FALSE(s.contains(at("2026-11-25", 17, 0)));
    CHECK_FALSE(s.contains(at("2026-11-28", 11, 0)));  // Saturday
    CHECK_FALSE(s.contains(at("2026-11-29", 11, 0)));  // Sunday
}

TEST_CASE("a custom schedule: Saturday mornings count as work") {
    WorkSchedule s;
    s.days        = 0b0100000;  // Saturday only
    s.startMinute = 8 * 60;
    s.endMinute   = 12 * 60;
    CHECK(s.contains(at("2026-11-28", 10, 0)));
    CHECK_FALSE(s.contains(at("2026-11-25", 10, 0)));  // a Wednesday is not in the mask
}

TEST_CASE("the policy picks the mode by the clock") {
    TrackingPolicy p;  // Work during work hours, Private otherwise
    CHECK(p.modeAt(at("2026-11-25", 10, 0)) == TrackingMode::Work);
    CHECK(p.modeAt(at("2026-11-25", 22, 0)) == TrackingMode::Private);
    CHECK(p.modeAt(at("2026-11-28", 10, 0)) == TrackingMode::Private);

    p.otherwise = TrackingMode::Off;
    CHECK(p.modeAt(at("2026-11-25", 22, 0)) == TrackingMode::Off);
}

TEST_CASE("a policy must make sense") {
    TrackingPolicy p;
    CHECK(valid(p));
    p.schedule.startMinute = 18 * 60;  // starts after it ends
    CHECK_FALSE(valid(p));
    p = TrackingPolicy{};
    p.schedule.endMinute = 25 * 60;
    CHECK_FALSE(valid(p));
    p = TrackingPolicy{};
    p.schedule.days = 0xFF;  // more than seven days
    CHECK_FALSE(valid(p));
}

TEST_CASE("mode names round-trip") {
    for (auto m : {TrackingMode::Off, TrackingMode::Private, TrackingMode::Work, TrackingMode::Detailed}) CHECK(parseMode(modeName(m)) == m);
    CHECK_FALSE(parseMode("everything").has_value());
}

TEST_CASE("profile text is trimmed and limited") {
    auto p = cleaned({"  student developer \n", std::string(500, 'x')});
    CHECK(p.role == "student developer");
    CHECK(p.purpose.size() == 240);
    CHECK(cleaned({"   ", ""}).role.empty());
}

TEST_CASE("modes: each level keeps strictly more than the one below it") {
    CHECK_FALSE(keepsCategory(TrackingMode::Off));
    CHECK_FALSE(keepsEngagement(TrackingMode::Off));

    CHECK(keepsCategory(TrackingMode::Private));
    CHECK(keepsEngagement(TrackingMode::Private));
    CHECK_FALSE(keepsAppName(TrackingMode::Private));
    CHECK_FALSE(keepsInputCounts(TrackingMode::Private));

    CHECK(keepsAppName(TrackingMode::Work));
    CHECK_FALSE(keepsInputCounts(TrackingMode::Work));  // counting scrolls and keys is opt-in, at the top level only

    CHECK(keepsAppName(TrackingMode::Detailed));
    CHECK(keepsInputCounts(TrackingMode::Detailed));
}
