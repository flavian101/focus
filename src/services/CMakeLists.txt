# Services: behaviours built on top of Core. Still portable C++.
add_library(focus_services STATIC
  ActivityClassifier.cpp
)
target_link_libraries(focus_services PUBLIC focus_core)
focus_enable_warnings(focus_services)
