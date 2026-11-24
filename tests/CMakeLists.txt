add_executable(focus_tests
  test_main.cpp
  test_activity_classifier.cpp
  test_controller.cpp
  test_day_plan.cpp
  test_scheduler.cpp
  test_session_manager.cpp
  test_statistics.cpp
  test_storage.cpp
  test_task_storage.cpp
  test_timer.cpp
  test_wellness.cpp
)
target_link_libraries(focus_tests PRIVATE focus_core focus_services focus_storage focus_controller doctest::doctest)
focus_enable_warnings(focus_tests)
add_test(NAME focus_tests COMMAND focus_tests)
