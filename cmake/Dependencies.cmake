# Third-party code is fetched at configure time with FetchContent.
include(FetchContent)

# Some third-party projects still declare an ancient cmake_minimum_required.
# CMake 4 refuses those unless we allow it explicitly. Harmless on older CMake.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(FOCUS_BUILD_TESTS)
  set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
  set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(doctest)
endif()
