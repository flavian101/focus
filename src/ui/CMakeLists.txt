# UI: Qt Quick front end. All the logic lives in focus_controller; this layer only draws it.
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Widgets)  # Widgets is only needed for the tray icon

set(CMAKE_AUTOMOC ON)  # Qt's "moc" generates the glue code behind Q_OBJECT / Q_PROPERTY

# Where QML files live inside the executable. Newer Qt versions use /qt/qml by default;
# stating it explicitly gives the same result on every Qt 6 version.
if(COMMAND qt_policy)
  qt_policy(SET QTP0001 NEW)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(focus_ui
  main.cpp
  FocusBackend.cpp
  UiApp.cpp
  SelfTest.cpp
)

qt_add_qml_module(focus_ui
  URI FocusUi
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  QML_FILES
    qml/Theme.qml
    qml/Txt.qml
    qml/Card.qml
    qml/StatCard.qml
    qml/PillButton.qml
    qml/Field.qml
    qml/MoodPicker.qml
    qml/ProgressRing.qml
    qml/WeekChart.qml
    qml/Sheet.qml
    qml/Toast.qml
    qml/StartSheet.qml
    qml/CheckInSheet.qml
    qml/Header.qml
    qml/TaskRow.qml
    qml/TodayPage.qml
    qml/InsightsPage.qml
    qml/SessionPage.qml
    qml/Main.qml
)

target_link_libraries(focus_ui PRIVATE Qt6::Quick Qt6::Widgets focus_controller)
focus_enable_warnings(focus_ui)

if(WIN32)
  target_link_libraries(focus_ui PRIVATE focus_platform)
  set_target_properties(focus_ui PROPERTIES WIN32_EXECUTABLE ON)  # a window app: no console box

  # Copy the Qt DLLs and plugins next to the .exe so it starts without Qt on PATH.
  if(TARGET Qt6::windeployqt)
    set(FOCUS_WINDEPLOYQT "$<TARGET_FILE:Qt6::windeployqt>")
  else()
    # <prefix>/lib/cmake/Qt6 -> <prefix>/bin
    find_program(FOCUS_WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
  endif()
  if(FOCUS_WINDEPLOYQT)
    add_custom_command(TARGET focus_ui POST_BUILD
      COMMAND "${FOCUS_WINDEPLOYQT}" --no-translations --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" "$<TARGET_FILE:focus_ui>"
      COMMENT "Deploying Qt runtime next to focus_ui.exe")
  else()
    message(WARNING "windeployqt not found: run it on focus_ui.exe yourself, or add Qt's bin folder to PATH")
  endif()
endif()

install(TARGETS focus_ui RUNTIME DESTINATION bin)

# Headless self-test: loads the UI, drives a whole session, writes screenshots.
if(FOCUS_BUILD_TESTS)
  add_test(NAME ui_self_test COMMAND focus_ui --self-test "${CMAKE_BINARY_DIR}/ui-screenshots")
  set_tests_properties(ui_self_test PROPERTIES TIMEOUT 120)
  if(NOT WIN32)
    # No display needed: render off-screen in software.
    set_tests_properties(ui_self_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  endif()
  # On Windows the test uses the normal window platform (a window flashes briefly), because
  # windeployqt does not ship Qt's "offscreen" plugin.
endif()
