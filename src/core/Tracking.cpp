#include "core/Tracking.h"

#include <algorithm>

namespace focus {

namespace {
constexpr std::size_t kMaxRole    = 80;
constexpr std::size_t kMaxPurpose = 240;

std::string trimmed(const std::string& s, std::size_t maxLength) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    std::string out = s.substr(first, last - first + 1);
    if (out.size() > maxLength) out.resize(maxLength);
    return out;
}
}  // namespace

int weekdayOfIso(const std::string& day) {
    if (day.size() != 10 || day[4] != '-' || day[7] != '-') return -1;
    auto number = [&](std::size_t from, std::size_t length) {
        int v = 0;
        for (std::size_t i = from; i < from + length; ++i) {
            if (day[i] < '0' || day[i] > '9') return -1;
            v = v * 10 + (day[i] - '0');
        }
        return v;
    };
    int y = number(0, 4);
    const int m = number(5, 2), d = number(8, 2);
    if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31) return -1;

    // "days from civil" (Howard Hinnant): days since 1970-01-01, which was a Thursday.
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long days = static_cast<long>(era) * 146097 + doe - 719468;
    return static_cast<int>(((days % 7) + 7 + 3) % 7);  // +3 because day 0 was a Thursday (Monday = 0)
}

bool WorkSchedule::contains(const LocalTime& t) const {
    const int weekday = weekdayOfIso(t.day);
    if (weekday < 0 || !(days & (1u << weekday))) return false;
    return t.minuteOfDay >= startMinute && t.minuteOfDay < endMinute;
}

bool valid(const TrackingPolicy& p) {
    const auto& s = p.schedule;
    return s.startMinute >= 0 && s.endMinute <= 24 * 60 && s.startMinute < s.endMinute && (s.days & ~0x7Fu) == 0;
}

UserProfile cleaned(UserProfile profile) {
    profile.role    = trimmed(profile.role, kMaxRole);
    profile.purpose = trimmed(profile.purpose, kMaxPurpose);
    return profile;
}

}  // namespace focus
