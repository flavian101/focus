#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace focus {

enum class CheckInKind { BeforeSession, AfterSession, Manual };

constexpr std::string_view kindName(CheckInKind k) {
    switch (k) {
        case CheckInKind::BeforeSession: return "before";
        case CheckInKind::AfterSession:  return "after";
        case CheckInKind::Manual:        return "manual";
    }
    return "manual";
}

inline std::optional<CheckInKind> parseCheckInKind(std::string_view s) {
    if (s == "before") return CheckInKind::BeforeSession;
    if (s == "after")  return CheckInKind::AfterSession;
    if (s == "manual") return CheckInKind::Manual;
    return std::nullopt;
}

// A moment of "how am I actually doing?". Two numbers plus an optional one-word label,
// kept deliberately cheap to fill in so you will actually do it.
struct CheckIn {
    std::int64_t id = 0;                      // set by storage
    std::optional<std::int64_t> sessionId;    // which session it belongs to, if any
    CheckInKind kind = CheckInKind::Manual;
    int mood   = 3;                           // 1 (awful) .. 5 (great)
    int energy = 3;                           // 1 (drained) .. 5 (energised)
    std::string tag;                          // e.g. "anxious", "stuck", "bored", "calm"
    std::string note;
    std::int64_t createdAt = 0;               // unix seconds

    bool isValid() const { return mood >= 1 && mood <= 5 && energy >= 1 && energy <= 5; }
};

}  // namespace focus
