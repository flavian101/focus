#pragma once
#include <chrono>

namespace focus::win {

// How long since the last keyboard or mouse input anywhere in this Windows session?
// Note what it cannot see: reading, watching a video or thinking all look like "idle".
// That is why the away threshold is minutes, not seconds.
std::chrono::seconds idleDuration();

}  // namespace focus::win
