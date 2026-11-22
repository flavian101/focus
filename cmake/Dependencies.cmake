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

# SQLite: either download the official "amalgamation" (one sqlite3.c file) and compile it
# ourselves, or use a copy already installed on the machine (vcpkg, apt, ...).
option(FOCUS_FETCH_SQLITE "Download the SQLite amalgamation (OFF = use find_package(SQLite3))" ON)

if(FOCUS_FETCH_SQLITE)
  FetchContent_Declare(
    sqlite_amalgamation
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(sqlite_amalgamation)  # the zip has no CMakeLists.txt, so this only downloads

  # Our project() line only enables C++, but sqlite3.c is C. Without a C compiler enabled,
  # CMake cannot even tell how to link the target ("can not determine linker language").
  enable_language(C)
  add_library(focus_sqlite3 STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
  target_include_directories(focus_sqlite3 PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
  if(UNIX)
    target_link_libraries(focus_sqlite3 PUBLIC dl pthread)
  endif()
  add_library(SQLite3::SQLite3 ALIAS focus_sqlite3)  # same name find_package gives us in CMake 4
else()
  find_package(SQLite3 REQUIRED)
  if(NOT TARGET SQLite3::SQLite3)  # older CMake only knows the SQLite::SQLite3 spelling
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
  endif()
endif()
