#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/ActivitySource.h"
#include "core/CheckIn.h"
#include "core/Configuration.h"
#include "core/Scheduler.h"
#include "core/SessionManager.h"
#include "core/Statistics.h"
#include "services/ActivityClassifier.h"
#include "storage/Database.h"

namespace focus {

struct Notification {
    std::string title;
    std::string body;
};

enum class Phase {
    Idle,      // nothing running
    Focusing,
    Paused,
    CheckIn,   // the session is over and saved; waiting for the "how was that?" check-in
};

// Everything a screen needs to draw itself, as plain values. A UI polls this about once a second.
struct ControllerSnapshot {
    Phase phase = Phase::Idle;
    std::string goal;
    std::string approach;
    std::int64_t plannedSeconds   = 0;
    std::int64_t remainingSeconds = 0;
    double progress               = 0.0;  // 0..1 of the planned time used
    std::array<std::int64_t, kActivityCategoryCount> secondsByCategory{};
    int aiUsed = 0;
    std::optional<int> aiBudget;
    bool present = true;  // false while away from the keyboard
    std::optional<SessionSummary> lastFinished;  // set once a session has ended, until the next start
};

// Orchestrates one session at a time: contract -> sampling -> reminders -> saving -> check-in.
// UI code calls the commands below and draws snapshot(); it never touches SessionManager,
// Scheduler or the database directly. Like the classes under it, it is handed "now".
class SessionController {
public:
    using Clock    = SessionManager::Clock;
    using NotifyFn = std::function<void(const Notification&)>;

    SessionController(Database& db, ActivitySource& source, Configuration config, NotifyFn notify = {});

    // `before` carries mood/energy/tag/note; kind, timestamp and session link are filled in here.
    // Returns false if a session is already running, the plan is invalid, or mood/energy are out of range.
    bool start(SessionPlan plan, CheckIn before, Clock::time_point now);
    void pause(Clock::time_point now);
    void resume(Clock::time_point now);
    void abandon(Clock::time_point now);
    void logAiUse(Clock::time_point now);  // for AI you use where we cannot see it (inside the editor, ...)

    // Call about once a second. Samples what the user is doing, fires reminders, ends the session on time.
    void tick(Clock::time_point now);

    // The session itself is saved the moment it ends; this adds the closing check-in.
    void submitAfterCheckIn(CheckIn after);
    void skipAfterCheckIn();

    ControllerSnapshot snapshot(Clock::time_point now) const;
    StatsReport stats(int days);
    std::vector<SessionSummary> recentSessions(int limit);

    const Configuration& config() const { return config_; }

private:
    void onSessionEvent(const SessionEvent& event);
    void sampleActivity(Clock::time_point now);
    void persistIfFinished();
    void notify(std::string title, std::string body) const;

    Database& db_;
    ActivitySource& source_;
    Configuration config_;
    NotifyFn notify_;

    ActivityClassifier classifier_;
    SessionManager session_;
    Scheduler scheduler_;

    CheckIn before_;
    bool present_            = true;
    bool finishedUnsaved_    = false;  // set by the session event, handled right after the call returns
    bool awaitingCheckIn_    = false;
    std::int64_t lastSessionId_ = 0;
    std::optional<SessionSummary> lastFinished_;
    Clock::time_point lastSampleAt_{};
    Clock::time_point eventNow_{};  // the "now" of the command being executed, for event handlers
};

}  // namespace focus
