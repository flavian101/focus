#include "platform/WindowDetection.h"

#include <windows.h>

#include <string>

#include "platform/Utf8.h"

namespace focus::win {

namespace {

std::wstring windowTitle(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');  // +1 for the terminating null
    const int copied = GetWindowTextW(hwnd, title.data(), length + 1);
    title.resize(static_cast<std::size_t>(copied > 0 ? copied : 0));
    return title;
}

std::wstring processFileName(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return {};

    // QUERY_LIMITED is the weakest permission that still works for most processes.
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    std::wstring path(1024, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    const bool ok = QueryFullProcessImageNameW(process, 0, path.data(), &size) != 0;
    CloseHandle(process);
    if (!ok) return {};

    path.resize(size);
    const auto slash = path.find_last_of(L'\\');
    return slash == std::wstring::npos ? path : path.substr(slash + 1);  // "C:\...\Code.exe" -> "Code.exe"
}

}  // namespace

std::optional<ForegroundWindow> queryForegroundWindow() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return std::nullopt;

    ForegroundWindow result;
    result.title       = toUtf8(windowTitle(hwnd));
    result.processName = toUtf8(processFileName(hwnd));
    return result;
}

}  // namespace focus::win
