#include "platform/InputCounter.h"

#include <windows.h>

#include <future>

namespace focus::win {

namespace {

// The hook callbacks are plain functions with nowhere to carry a pointer, so the running counter is found here.
std::atomic<InputCounter::Counters*> g_counters{nullptr};

// NOTE for anyone editing these two functions: they must stay this small and must not look at lParam. lParam
// points at the key code / pointer position, which this app deliberately never reads. They also run inside
// Windows' input path, so anything slow here would make the whole desktop feel laggy.
LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        if (auto* c = g_counters.load(std::memory_order_relaxed)) {
            switch (wParam) {
                case WM_MOUSEWHEEL:
                case WM_MOUSEHWHEEL:
                    c->scrolls.fetch_add(1, std::memory_order_relaxed);
                    break;
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                    c->clicks.fetch_add(1, std::memory_order_relaxed);
                    break;
                default:
                    break;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        if (auto* c = g_counters.load(std::memory_order_relaxed)) c->keys.fetch_add(1, std::memory_order_relaxed);
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

bool InputCounter::start() {
    if (thread_.joinable()) return true;

    Counters* expected = nullptr;
    if (!g_counters.compare_exchange_strong(expected, &counters_)) return false;  // another counter is running
    counters_.scrolls = 0;
    counters_.keys    = 0;
    counters_.clicks  = 0;

    std::promise<bool> ready;
    auto result = ready.get_future();
    thread_     = std::thread(&InputCounter::threadMain, this, static_cast<void*>(&ready));
    if (!result.get()) {  // the thread reports whether the hooks went in, then (on failure) ends by itself
        thread_.join();
        g_counters.store(nullptr);
        return false;
    }
    return true;
}

void InputCounter::stop() {
    if (!thread_.joinable()) return;
    PostThreadMessageW(threadId_.load(), WM_QUIT, 0, 0);  // ends the hook thread's message loop
    thread_.join();                                       // the thread removes both hooks before it returns
    threadId_ = 0;
    g_counters.store(nullptr);
}

InputCounts InputCounter::take() {
    InputCounts out;
    out.scrolls = counters_.scrolls.exchange(0);
    out.keys    = counters_.keys.exchange(0);
    out.clicks  = counters_.clicks.exchange(0);
    return out;
}

void InputCounter::threadMain(void* readyPromise) {
    auto* ready = static_cast<std::promise<bool>*>(readyPromise);

    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  // makes sure this thread has a message queue before anyone posts to it

    const HINSTANCE module = GetModuleHandleW(nullptr);
    HHOOK mouse            = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, module, 0);
    HHOOK keyboard         = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, module, 0);
    if (!mouse || !keyboard) {
        if (mouse) UnhookWindowsHookEx(mouse);
        if (keyboard) UnhookWindowsHookEx(keyboard);
        ready->set_value(false);
        return;
    }
    threadId_ = GetCurrentThreadId();
    ready->set_value(true);  // after this the promise belongs to start() again: do not touch it

    // Low-level hooks are delivered through this thread's message loop.
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnhookWindowsHookEx(mouse);
    UnhookWindowsHookEx(keyboard);
}

}  // namespace focus::win
