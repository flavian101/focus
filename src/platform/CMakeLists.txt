# Platform: the only layer that talks to Win32. It converts everything to plain
# C++ types (UTF-8 std::string, std::optional, ...) before handing data upward.
add_library(focus_platform STATIC
  Utf8.cpp
  Paths.cpp
  WindowDetection.cpp
  SystemTray.cpp
  Hotkeys.cpp
  IdleDetection.cpp
  WindowsActivitySource.cpp
)
target_link_libraries(focus_platform PUBLIC focus_core PRIVATE user32 shell32 ole32)
target_compile_definitions(focus_platform PUBLIC
  WIN32_LEAN_AND_MEAN  # skip rarely used Windows headers: faster builds, fewer name clashes
  NOMINMAX             # windows.h defines min/max macros that break std::min/std::max
  UNICODE _UNICODE)    # make the generic API names (CreateWindow, ...) mean the wide "W" versions
focus_enable_warnings(focus_platform)
