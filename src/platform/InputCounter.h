#pragma once
#include <atomic>
#include <thread>

#include "core/ActivitySource.h"  // InputCounts

namespace focus::win {

// Counts scrolls, key presses and mouse clicks across the whole desktop, and nothing else.
//
// How: two low-level hooks (WH_MOUSE_LL, WH_KEYBOARD_LL) on a thread of their own. A hook is used rather than
// Raw Input because precision touchpads turn two-finger scrolling into wheel messages at a level Raw Input never
// sees, and on a laptop that is most scrolling.
//
// Privacy: each hook callback decides from the MESSAGE ID alone ("was that a wheel tick? a key going down? a
// button press?") and adds one to a counter. The callbacks never read lParam, which is the only place the key
// code, the pointer position and the wheel direction live. So there is nothing to leak, and nothing to store.
//
// It is off until start() and fully off again after stop(): while stopped, no hook is installed and no code of
// ours sees any input. Only one counter can run at a time (the hook callbacks have no place to carry a pointer).
class InputCounter {
public:
    struct Counters {
        std::atomic<int> scrolls{0};
        std::atomic<int> keys{0};
        std::atomic<int> clicks{0};
    };

    InputCounter() = default;
    ~InputCounter() { stop(); }
    InputCounter(const InputCounter&)            = delete;
    InputCounter& operator=(const InputCounter&) = delete;

    // Installs the hooks. Returns true if counting is running afterwards (also if it already was); false if
    // Windows refused or another counter is already running. Counts start again from zero.
    bool start();
    void stop();
    bool running() const { return thread_.joinable(); }

    // What was counted since the previous call (or since start()), then starts again from zero.
    InputCounts take();

private:
    void threadMain(void* readyPromise);

    Counters counters_;
    std::thread thread_;
    std::atomic<unsigned long> threadId_{0};  // the hook thread's id, so stop() can tell its message loop to end
};

}  // namespace focus::win
