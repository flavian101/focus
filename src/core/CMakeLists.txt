# Core: pure, portable C++ (no Win32 includes). Everything else depends on this.
add_library(focus_core STATIC
  ActivityTracker.cpp
  DaySummary.cpp
  DayPlan.cpp
  Scheduler.cpp
  SessionManager.cpp
  Statistics.cpp
  Timer.cpp
  Tracking.cpp
  Wellness.cpp
)
# Include root is src/, so other code writes #include "core/Timer.h".
target_include_directories(focus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(focus_core PUBLIC cxx_std_20)
focus_enable_warnings(focus_core)
