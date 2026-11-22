#include "platform/SystemTray.h"

#include "platform/Utf8.h"

namespace focus::win {

namespace {
constexpr UINT kIconId = 1;  // identifies our icon among the ones this window owns
}

TrayIcon::TrayIcon(HWND owner, UINT callbackMessage) {
    data_.cbSize           = sizeof(data_);  // Windows uses this to know which struct version we mean
    data_.hWnd             = owner;
    data_.uID              = kIconId;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);  // stock icon for now; a custom .ico comes later
}

TrayIcon::~TrayIcon() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
}

bool TrayIcon::add(const std::string& tooltipUtf8) {
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    lstrcpynW(data_.szTip, toWide(tooltipUtf8).c_str(), static_cast<int>(std::size(data_.szTip)));
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    return added_;
}

void TrayIcon::setTooltip(const std::string& tooltipUtf8) {
    if (!added_) return;
    data_.uFlags = NIF_TIP;
    lstrcpynW(data_.szTip, toWide(tooltipUtf8).c_str(), static_cast<int>(std::size(data_.szTip)));
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

void TrayIcon::notify(const std::string& titleUtf8, const std::string& bodyUtf8) {
    if (!added_) return;
    data_.uFlags      = NIF_INFO;
    data_.dwInfoFlags = NIIF_INFO;
    lstrcpynW(data_.szInfoTitle, toWide(titleUtf8).c_str(), static_cast<int>(std::size(data_.szInfoTitle)));
    lstrcpynW(data_.szInfo, toWide(bodyUtf8).c_str(), static_cast<int>(std::size(data_.szInfo)));
    Shell_NotifyIconW(NIM_MODIFY, &data_);
}

}  // namespace focus::win
