add_executable(focus_tests
  test_main.cpp
  test_timer.cpp
)
target_link_libraries(focus_tests PRIVATE focus_core doctest::doctest)
focus_enable_warnings(focus_tests)
add_test(NAME focus_tests COMMAND focus_tests)
