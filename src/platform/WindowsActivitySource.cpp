#include "platform/WindowsActivitySource.h"

#include "platform/IdleDetection.h"
#include "platform/WindowDetection.h"

namespace focus::win {

std::optional<ForegroundWindow> WindowsActivitySource::foreground() { return queryForegroundWindow(); }

std::chrono::seconds WindowsActivitySource::idleFor() { return idleDuration(); }

}  // namespace focus::win
