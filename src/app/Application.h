#pragma once
#include <windows.h>

#include <memory>

#include "core/CheckIn.h"
#include "core/Configuration.h"
#include "core/SessionManager.h"
#include "platform/Hotkeys.h"
#include "platform/SystemTray.h"
#include "services/ActivityClassifier.h"
#include "storage/Database.h"

namespace focus {

// Wires Core + Services + Storage + Platform together for one focus session.
//
// A Windows program is driven by MESSAGES: timers, hotkeys and tray clicks all arrive as
// messages for a window. We create a hidden window purely to receive them, then run the
// standard message loop until the session ends.
class Application {
public:
    Application(Database& db, Configuration config);
    ~Application();

    // Runs one session start to finish: check-in, countdown, check-in, save.
    // Blocks until the session completes or is abandoned. Returns a process exit code.
    int runSession(SessionPlan plan);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool createWindow();
    void onTick();
    void onHotkey(int id);
    void onSessionEvent(const SessionEvent& event);
    void togglePause();
    std::string tooltip() const;
    void save(const CheckIn& before, const CheckIn& after);

    Database& db_;
    Configuration config_;
    ActivityClassifier classifier_;
    SessionManager session_;

    HWND hwnd_ = nullptr;
    std::unique_ptr<win::TrayIcon> tray_;
    std::unique_ptr<win::HotkeyManager> hotkeys_;
    SessionManager::Clock::time_point lastSampleAt_{};
};

}  // namespace focus
