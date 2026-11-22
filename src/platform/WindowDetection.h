#pragma once
#include <optional>

#include "core/Activity.h"

namespace focus::win {

// Which app and window title is in front right now?
// Returns nullopt when there is no foreground window (lock screen, mid-switch, ...).
// processName may be empty if Windows denies access (e.g. an elevated/admin app);
// the title is still filled in, so title-based rules keep working.
std::optional<ForegroundWindow> queryForegroundWindow();

}  // namespace focus::win
