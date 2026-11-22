# App: the composition root. It creates the pieces from the other layers and wires them together.
add_executable(focus
  main.cpp
  Application.cpp
  Console.cpp
  Report.cpp
)
target_link_libraries(focus PRIVATE focus_core focus_services focus_storage focus_platform)
focus_enable_warnings(focus)
if(MINGW)
  target_link_options(focus PRIVATE -municode)  # needed for wmain() (UTF-16 command line) under MinGW
endif()
