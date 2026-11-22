add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(storage)

# Win32 code only builds on Windows. Everything above stays portable and testable anywhere.
if(WIN32)
  add_subdirectory(platform)
  add_subdirectory(app)
endif()
