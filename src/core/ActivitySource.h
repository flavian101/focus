#pragma once
#include <chrono>
#include <optional>

#include "core/Activity.h"

namespace focus {

// "What is the user doing right now?" asked as a question, so the controller does not care
// whether the answer comes from Win32, a test double, or a platform that has no answer.
class ActivitySource {
public:
    virtual ~ActivitySource() = default;
    virtual std::optional<ForegroundWindow> foreground() = 0;
    virtual std::chrono::seconds idleFor() = 0;  // time since last keyboard/mouse input
};

// For platforms without detection (and for development on non-Windows machines).
class NullActivitySource final : public ActivitySource {
public:
    std::optional<ForegroundWindow> foreground() override { return std::nullopt; }
    std::chrono::seconds idleFor() override { return std::chrono::seconds::zero(); }
};

}  // namespace focus
