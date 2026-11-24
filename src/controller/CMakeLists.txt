# Controller: the UI-agnostic "brain" that any front end (console, Qt, ...) drives.
# Portable: no Win32, no Qt. Windows specifics arrive through core/ActivitySource.h.
add_library(focus_controller STATIC
  SessionController.cpp
)
target_link_libraries(focus_controller PUBLIC focus_core focus_services focus_storage)
focus_enable_warnings(focus_controller)
