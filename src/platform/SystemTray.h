#pragma once
#include <windows.h>
#include <shellapi.h>

#include <string>

namespace focus::win {

// A notification-area icon that can also pop up notifications.
// The icon needs a window to send its events to (see `callbackMessage`).
class TrayIcon {
public:
    // callbackMessage: a WM_APP+n value. Windows posts it to `owner` for mouse events on the icon.
    TrayIcon(HWND owner, UINT callbackMessage);
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    ~TrayIcon();  // removes the icon; otherwise a "ghost" stays until you hover over it

    bool add(const std::string& tooltipUtf8);
    void setTooltip(const std::string& tooltipUtf8);

    // Pops a notification next to the tray. On Windows 10/11 it appears as a toast.
    void notify(const std::string& titleUtf8, const std::string& bodyUtf8);

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace focus::win
