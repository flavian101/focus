#pragma once
#include <windows.h>

#include <vector>

namespace focus::win {

// System-wide keyboard shortcuts: they fire even when another app has focus.
// When one is pressed, Windows posts WM_HOTKEY to `owner`, with the id you chose in wParam.
class HotkeyManager {
public:
    explicit HotkeyManager(HWND owner) : owner_(owner) {}
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    ~HotkeyManager();

    // modifiers: MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN. virtualKey: e.g. 'P' or VK_F9.
    // Returns false if another program already owns that combination.
    bool add(int id, UINT modifiers, UINT virtualKey);

private:
    HWND owner_;
    std::vector<int> ids_;
};

}  // namespace focus::win
