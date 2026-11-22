#pragma once
#include <chrono>
#include <string>
#include <vector>

namespace focus {

// "Every <interval>, tell the user <message>."
struct Reminder {
    std::string id;       // stable name, e.g. "eye-care"
    std::string title;
    std::string message;
    std::chrono::seconds interval{0};
};

// Decides WHEN reminders are due. It does not show anything: the caller asks "what is due now?"
// and decides how to display it. Like Timer and SessionManager it never reads the clock itself.
class Scheduler {
public:
    using Clock = std::chrono::steady_clock;

    // Returns false (and ignores it) if the interval is not positive.
    bool add(Reminder reminder);

    void start(Clock::time_point now);  // arm every reminder: first one fires one interval from now
    void stop();
    void pause(Clock::time_point now);
    void resume(Clock::time_point now);

    // Reminders that are due at `now`. Each one returned is re-armed for one interval from NOW,
    // so a long stall (laptop asleep, app frozen) produces one reminder, not a burst of catch-up ones.
    std::vector<Reminder> collectDue(Clock::time_point now);

    bool isRunning() const { return running_; }

private:
    struct Entry {
        Reminder reminder;
        Clock::time_point nextDue{};
    };

    std::vector<Entry> entries_;
    bool running_ = false;
    bool paused_  = false;
    Clock::time_point pausedAt_{};
};

}  // namespace focus
