#include "services/Reminders.h"

namespace focus {

std::vector<Reminder> defaultReminders() {
    using namespace std::chrono_literals;
    return {
        {"eye-care", "Eye break", "Look at something about 6 m (20 ft) away for 20 seconds.", 20min},
        {"stretch", "Stretch", "Stand up. Roll your shoulders and stretch your wrists and back.", 45min},
        {"hydration", "Water", "Drink some water.", 60min},
    };
}

}  // namespace focus
