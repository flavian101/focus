cmake_minimum_required(VERSION 3.24)
project(Focus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FOCUS_BUILD_TESTS "Build the unit tests" ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(FOCUS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
