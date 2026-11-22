#include "platform/IdleDetection.h"

#include <windows.h>

namespace focus::win {

std::chrono::seconds idleDuration() {
    LASTINPUTINFO info{};
    info.cbSize = sizeof(info);  // Windows requires us to say how big the struct is
    if (!GetLastInputInfo(&info)) return std::chrono::seconds::zero();

    // Both values are 32-bit "milliseconds since boot" counters that wrap around every ~49.7 days.
    // Unsigned subtraction gives the right answer even across a wrap.
    const DWORD idleMs = GetTickCount() - info.dwTime;
    return std::chrono::seconds(idleMs / 1000);
}

}  // namespace focus::win
