#pragma once
#include <string>
#include <string_view>

namespace focus::win {

// Windows APIs speak UTF-16 (wchar_t). The rest of this project speaks UTF-8 (char).
// These two functions are the only bridge, so encoding bugs have one place to live.
std::string toUtf8(std::wstring_view wide);
std::wstring toWide(std::string_view utf8);

}  // namespace focus::win
