#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/ActivitySource.h"
#include "core/CheckIn.h"
#include "core/ActivityTracker.h"
#include "core/Configuration.h"
#include "core/DaySummary.h"
#include "core/DayPlan.h"
#include "core/Task.h"
#include "core/Tracking.h"
#include "core/Wellness.h"
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

// The outcome of a plan edit: either it worked, or `message` says why not, in words for the user.
struct TaskResult {
    bool ok = false;
    std::string message;
    std::int64_t id = 0;  // the task that was created or changed
};

struct MealView {
    std::string id;
    std::string name;
    int startMinute = 0;
    int endMinute   = 0;
    MealState state = MealState::Upcoming;
};

struct WellnessSnapshot {
    int glasses  = 0;
    int goal     = 8;
    int expected = 0;  // where you should be by now to be on pace
    int dayStartMinute = 0;
    int dayEndMinute   = 0;
    std::vector<MealView> meals;
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

    // ---- the day plan. `day` is the caller's local date as "YYYY-MM-DD". ----------------
    TaskResult addTask(const std::string& day, std::string title, Priority priority, int estimateMinutes);
    TaskResult setTaskPriority(std::int64_t id, Priority priority);
    void setTaskStatus(std::int64_t id, TaskStatus status);
    // Moves unfinished tasks from earlier days onto `today`. If that would break the must-do
    // limit, the extra ones come over as "Should". Returns how many tasks moved.
    int carryOverUnfinished(const std::string& today);
    int unfinishedBefore(const std::string& day);  // how many tasks carryOverUnfinished would move
    std::vector<Task> tasksFor(const std::string& day);
    std::optional<Task> task(std::int64_t id);
    PlanSummary planFor(const std::string& day);

    // ---- water and meals: all day, with or without a session. `local` is the caller's clock. ----
    void logWater(const LocalTime& local);
    bool undoWater(const LocalTime& local);
    void setMeal(const LocalTime& local, const std::string& mealId, std::optional<MealStatus> status);  // nullopt = clear
    void setWaterGoal(int glasses);  // saved between runs
    WellnessSnapshot wellness(const LocalTime& local);
    // Call about once a second. It looks at most every 30 seconds, and shows reminders through the
    // notify callback only while the user is at the keyboard.
    void tickWellness(const LocalTime& local, Clock::time_point now);

    // ---- all-day activity: category and app name per minute, with or without a session ----------
    // Call about once a second. It samples every few seconds and saves finished minutes every minute.
    void trackActivity(const LocalTime& local, Clock::time_point now);
    void flushActivity();  // save everything still buffered: call when the app quits
    bool trackingEnabled() const { return trackingEnabled_; }  // the master pause switch

    // How much detail is kept right now: the pause switch wins (Off), otherwise the policy decides by the clock.
    TrackingMode currentTrackingMode(const LocalTime& local) const;
    const TrackingPolicy& trackingPolicy() const { return policy_; }
    bool setTrackingPolicy(const TrackingPolicy& policy);  // false (and unchanged) if it does not make sense; saved otherwise

    // Free text about the user, kept on this computer. Gives later features context.
    const UserProfile& profile() const { return profile_; }
    void setProfile(UserProfile profile);

    // "This app is <category>". Applies from now on and re-labels today's recorded time.
    // `appName` may be "Code.exe" or "code". Setting a category of Away is ignored.
    void setAppCategory(const std::string& appName, ActivityCategory category, const std::string& day);
    void clearAppCategory(const std::string& appName);
    const std::map<std::string, ActivityCategory>& appRules() const { return appRules_; }
    void setTrackingEnabled(bool enabled);  // saved; pausing does not credit the time it was off
    DaySummary daySummary(const std::string& day);  // what is saved plus what is still buffered
    void clearActivity(const std::string& day);

    ControllerSnapshot snapshot(Clock::time_point now) const;
    StatsReport stats(int days);
    std::vector<SessionSummary> recentSessions(int limit);

    const Configuration& config() const { return config_; }
    PlanRules planRules() const;

private:
    void onSessionEvent(const SessionEvent& event);
    void loadTrackingSettings();
    void updateInputCounting(bool wanted);  // tells the source to start or stop counting scrolls, keys and clicks
    void sampleActivity(Clock::time_point now);
    void persistIfFinished();
    void notify(std::string title, std::string body) const;

    Database& db_;
    ActivitySource& source_;
    Configuration config_;
    NotifyFn notify_;

    TrackingPolicy policy_;
    UserProfile profile_;
    std::map<std::string, ActivityCategory> appRules_;

    ActivityTracker tracker_;
    bool trackingEnabled_ = true;
    bool inputCounting_   = false;  // whether the source has been asked to count input events
    bool haveForegroundKey_ = false;
    std::size_t foregroundKey_ = 0;  // hash of the last foreground window, only to notice a change; never stored, never logged
    bool ambientSampled_  = false;
    Clock::time_point lastAmbient_{};
    Clock::time_point lastFlush_{};

    WellnessEngine wellness_;
    bool wellnessChecked_ = false;
    Clock::time_point lastWellnessCheck_{};

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
