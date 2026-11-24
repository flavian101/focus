#pragma once
#include "core/ActivitySource.h"

namespace focus::win {

// The real thing: foreground window + idle time straight from Windows.
class WindowsActivitySource final : public ActivitySource {
public:
    std::optional<ForegroundWindow> foreground() override;
    std::chrono::seconds idleFor() override;
};

}  // namespace focus::win
