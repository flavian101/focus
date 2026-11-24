# Storage: SQLite persistence. Portable; the only place that includes <sqlite3.h>.
add_library(focus_storage STATIC
  Database.cpp
  Migrations.cpp
  SessionRepository.cpp
  CheckInRepository.cpp
  TaskRepository.cpp
  WellnessRepository.cpp
  SettingsRepository.cpp
)
target_link_libraries(focus_storage PUBLIC focus_core PRIVATE SQLite3::SQLite3)
focus_enable_warnings(focus_storage)
