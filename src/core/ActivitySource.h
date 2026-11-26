#pragma once
#include <chrono>
#include <optional>

#include "core/Activity.h"

namespace focus {

// How many scrolls, key presses and clicks happened since the last time they were asked for.
// Counts only: which keys, what was typed and where the pointer was are not available here, by design.
struct InputCounts {
    int scrolls = 0;
    int keys    = 0;
    int clicks  = 0;
};

// "What is the user doing right now?" asked as a question, so the controller does not care
// whether the answer comes from Win32, a test double, or a platform that has no answer.
class ActivitySource {
public:
    virtual ~ActivitySource() = default;
    virtual std::optional<ForegroundWindow> foreground() = 0;
    virtual std::chrono::seconds idleFor() = 0;  // time since last keyboard/mouse input

    // Input counting is opt-in and off by default. The controller turns it on only while the tracking mode
    // allows it (Detailed), and off again the moment it does not. A source that cannot count ignores both calls.
    virtual void setInputCounting(bool /*enabled*/) {}
    virtual InputCounts takeInputCounts() { return {}; }  // returns what happened since the last call, then starts again from zero
};

// For platforms without detection (and for development on non-Windows machines).
class NullActivitySource final : public ActivitySource {
public:
    std::optional<ForegroundWindow> foreground() override { return std::nullopt; }
    std::chrono::seconds idleFor() override { return std::chrono::seconds::zero(); }
};

}  // namespace focus
