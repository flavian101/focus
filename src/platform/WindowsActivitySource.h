#pragma once
#include "core/ActivitySource.h"
#include "platform/InputCounter.h"

namespace focus::win {

// The real thing: foreground window + idle time straight from Windows.
// Scroll / key / click counts come from InputCounter, which is only running while the controller has asked for it.
class WindowsActivitySource final : public ActivitySource {
public:
    std::optional<ForegroundWindow> foreground() override;
    std::chrono::seconds idleFor() override;

    void setInputCounting(bool enabled) override;
    InputCounts takeInputCounts() override;

private:
    InputCounter counter_;
};

}  // namespace focus::win
