#include "platform/Paths.h"

#include <windows.h>
#include <shlobj.h>

#include "platform/Utf8.h"

namespace focus::win {

std::string appDataDirectory() {
    PWSTR raw = nullptr;
    // The shell allocates the string; we must free it with CoTaskMemFree.
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        CoTaskMemFree(raw);
        return {};
    }
    std::wstring dir = std::wstring(raw) + L"\\Focus";
    CoTaskMemFree(raw);

    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
    return toUtf8(dir);
}

}  // namespace focus::win
