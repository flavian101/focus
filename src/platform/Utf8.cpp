#include "platform/Utf8.h"

#include <windows.h>

namespace focus::win {

std::string toUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    // First call asks "how many bytes will I need?", second call does the conversion.
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string out(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                        out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (chars <= 0) return {};
    std::wstring out(static_cast<std::size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), chars);
    return out;
}

}  // namespace focus::win
