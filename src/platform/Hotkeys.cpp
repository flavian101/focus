#include "platform/Hotkeys.h"

namespace focus::win {

HotkeyManager::~HotkeyManager() {
    for (int id : ids_) UnregisterHotKey(owner_, id);
}

bool HotkeyManager::add(int id, UINT modifiers, UINT virtualKey) {
    // MOD_NOREPEAT: holding the keys down fires once, not 30 times per second.
    if (!RegisterHotKey(owner_, id, modifiers | MOD_NOREPEAT, virtualKey)) return false;
    ids_.push_back(id);
    return true;
}

}  // namespace focus::win
