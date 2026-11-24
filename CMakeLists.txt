cmake_minimum_required(VERSION 3.24)
project(Focus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FOCUS_BUILD_TESTS "Build the unit tests" ON)
option(FOCUS_BUILD_UI "Build the Qt Quick user interface (needs Qt 6.4 or newer)" OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

# enable_testing() must come BEFORE any add_subdirectory() that declares tests,
# otherwise CMake silently drops those tests (src/ui adds one).
if(FOCUS_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(src)

if(FOCUS_BUILD_TESTS)
  add_subdirectory(tests)
endif()
