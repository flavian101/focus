#pragma once
#include <vector>

#include "core/Scheduler.h"

namespace focus {

// The built-in reminders that run during a focus session: Eye Care (20-20-20 rule) and Stretch.
// Water and meals are handled all day by WellnessEngine instead, not only inside sessions.
std::vector<Reminder> defaultReminders();

}  // namespace focus
