#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace focus {

inline std::int64_t unixNow() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// 1500 -> "25m", 4500 -> "1h 15m"
inline std::string formatDuration(std::int64_t seconds) {
    const auto minutes = seconds / 60;
    if (minutes >= 60) return std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m";
    return std::to_string(minutes) + "m";
}

}  // namespace focus
