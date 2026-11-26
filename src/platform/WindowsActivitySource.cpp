#include "platform/WindowsActivitySource.h"

#include "platform/IdleDetection.h"
#include "platform/WindowDetection.h"

namespace focus::win {

std::optional<ForegroundWindow> WindowsActivitySource::foreground() { return queryForegroundWindow(); }

std::chrono::seconds WindowsActivitySource::idleFor() { return idleDuration(); }

void WindowsActivitySource::setInputCounting(bool enabled) {
    if (enabled) counter_.start();  // if Windows refuses, the counts simply stay at zero
    else counter_.stop();
}

InputCounts WindowsActivitySource::takeInputCounts() { return counter_.take(); }

}  // namespace focus::win
