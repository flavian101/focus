#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "core/Activity.h"
#include "core/Timer.h"

namespace focus {

enum class SessionState { Idle, Focusing, Paused };
enum class SessionOutcome { Completed, Abandoned };

// The "contract" you make with yourself before a session begins.
struct SessionPlan {
    std::string goal;      // the specific thing you will get done
    std::string approach;  // what you will try on your own BEFORE asking anyone or any AI
    std::chrono::minutes duration{25};
    std::optional<int> aiBudget;  // max AI consultations this session; nullopt = no limit
};

// What happened. Live while a session runs, final once it ends.
struct SessionSummary {
    std::int64_t id = 0;  // set by storage
    std::string goal;
    std::string approach;
    std::int64_t plannedSeconds = 0;
    std::int64_t activeSeconds = 0;  // time actually spent focusing (pauses excluded)
    std::int64_t startedAt = 0;      // unix seconds, wall clock
    std::int64_t endedAt = 0;
    SessionOutcome outcome = SessionOutcome::Abandoned;
    std::optional<int> aiBudget;
    int aiVisits = 0;      // detected: switched to an AI tool window
    int aiManualLogs = 0;  // self-reported (e.g. AI inside the editor, which we can't see)
    std::array<std::int64_t, kActivityCategoryCount> secondsByCategory{};

    std::int64_t secondsIn(ActivityCategory c) const {
        return secondsByCategory[static_cast<std::size_t>(c)];
    }
    int aiTotal() const { return aiVisits + aiManualLogs; }
};

struct SessionEvent {
    enum class Type { Started, Paused, Resumed, Completed, Abandoned, AiVisit, AiBudgetExceeded };
    Type type;
};

// State machine for one focus session at a time:  Idle -> Focusing <-> Paused -> Idle
//
// Like Timer, it never reads the clock itself; callers pass "now" in.
// It does not know about Win32, SQLite or UI. It only emits events,
// and whoever owns it decides what to do with them.
class SessionManager {
public:
    using Clock    = Timer::Clock;
    using Listener = std::function<void(const SessionEvent&)>;

    explicit SessionManager(Listener listener = {}) : listener_(std::move(listener)) {}

    // Returns false (and changes nothing) if a session is already active,
    // the goal is empty, or the duration is not positive.
    bool start(SessionPlan plan, Clock::time_point now);
    void pause(Clock::time_point now);
    void resume(Clock::time_point now);
    void tick(Clock::time_point now);  // call regularly; completes the session when time is up
    void abandon(Clock::time_point now);

    // Feed in what the user is looking at. Time since the previous sample is credited
    // to the PREVIOUS category ("sample and hold").
    void recordActivity(ActivityCategory category, Clock::time_point now);
    void logManualAiUse(Clock::time_point now);

    // "The user has had no keyboard/mouse input for idleFor." Call it instead of recordActivity
    // while idle. The whole idle stretch is moved to the Away category, even the part that was
    // already credited before we knew they had left.
    void recordAway(std::chrono::seconds idleFor, Clock::time_point now);

    SessionState state() const { return state_; }
    bool isActive() const { return state_ != SessionState::Idle; }
    std::chrono::seconds remaining(Clock::time_point now) const { return timer_.remaining(now); }
    const SessionSummary& current() const { return summary_; }

private:
    void creditElapsed(Clock::time_point now);
    void refreshSummarySeconds();
    void checkAiBudget();
    void finish(SessionOutcome outcome, Clock::time_point now);
    void emit(SessionEvent::Type type) const;

    Listener listener_;
    Timer timer_;
    SessionState state_ = SessionState::Idle;
    SessionSummary summary_;

    // Per-category time is accumulated at full clock precision and only converted to whole
    // seconds for the summary. Truncating every sample would slowly lose time.
    std::array<Clock::duration, kActivityCategoryCount> spent_{};
    ActivityCategory lastCategory_ = ActivityCategory::Neutral;
    ActivityCategory categoryBeforeAway_ = ActivityCategory::Neutral;  // so coming back is not a "new" AI visit
    Clock::time_point lastSampleAt_{};
    bool budgetExceededEmitted_ = false;
};

}  // namespace focus
