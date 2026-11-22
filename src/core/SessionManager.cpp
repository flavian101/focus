#include "core/SessionManager.h"

#include <algorithm>

namespace focus {

namespace {
std::int64_t unixNow() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
}  // namespace

bool SessionManager::start(SessionPlan plan, Clock::time_point now) {
    if (isActive() || plan.goal.empty() || plan.duration <= std::chrono::minutes::zero()) return false;

    summary_ = SessionSummary{};
    summary_.goal           = std::move(plan.goal);
    summary_.approach       = std::move(plan.approach);
    summary_.plannedSeconds = std::chrono::duration_cast<std::chrono::seconds>(plan.duration).count();
    summary_.aiBudget       = plan.aiBudget;
    summary_.startedAt      = unixNow();

    spent_.fill(Clock::duration::zero());
    lastCategory_         = ActivityCategory::Neutral;
    categoryBeforeAway_   = ActivityCategory::Neutral;
    lastSampleAt_         = now;
    budgetExceededEmitted_ = false;

    timer_.start(std::chrono::duration_cast<Timer::Seconds>(plan.duration), now);
    state_ = SessionState::Focusing;
    notifyListener(SessionEvent::Type::Started);
    return true;
}

void SessionManager::pause(Clock::time_point now) {
    if (state_ != SessionState::Focusing) return;
    creditElapsed(now);  // settle up before the clock stops counting
    timer_.pause(now);
    state_ = SessionState::Paused;
    notifyListener(SessionEvent::Type::Paused);
}

void SessionManager::resume(Clock::time_point now) {
    if (state_ != SessionState::Paused) return;
    timer_.resume(now);
    lastSampleAt_ = now;  // time spent paused is credited to nobody
    state_        = SessionState::Focusing;
    notifyListener(SessionEvent::Type::Resumed);
}

void SessionManager::tick(Clock::time_point now) {
    if (state_ == SessionState::Focusing && timer_.expired(now)) {
        creditElapsed(now);
        finish(SessionOutcome::Completed, now);
    }
}

void SessionManager::abandon(Clock::time_point now) {
    if (!isActive()) return;
    if (state_ == SessionState::Focusing) creditElapsed(now);
    finish(SessionOutcome::Abandoned, now);
}

void SessionManager::recordActivity(ActivityCategory category, Clock::time_point now) {
    if (state_ != SessionState::Focusing) return;
    creditElapsed(now);

    // A "visit" is a switch INTO an AI tool, not every sample spent inside it.
    // Walking away and coming back to the same AI window is not a new visit.
    const auto before = lastCategory_ == ActivityCategory::Away ? categoryBeforeAway_ : lastCategory_;
    if (category == ActivityCategory::AiAssistant && before != ActivityCategory::AiAssistant) {
        ++summary_.aiVisits;
        notifyListener(SessionEvent::Type::AiVisit);
        checkAiBudget();
    }
    lastCategory_ = category;
}

void SessionManager::recordAway(std::chrono::seconds idleFor, Clock::time_point now) {
    if (state_ != SessionState::Focusing) return;
    creditElapsed(now);  // settle everything up to now into the current category first

    if (lastCategory_ != ActivityCategory::Away) {
        // First sample of this absence. Without input the foreground window cannot have changed,
        // so the whole idle stretch was spent in lastCategory_. Move it over, capped at what that
        // category actually has (the idle period may have begun before the session did).
        const auto from = static_cast<std::size_t>(lastCategory_);
        const auto to   = static_cast<std::size_t>(ActivityCategory::Away);
        const auto move = std::min(std::chrono::duration_cast<Clock::duration>(idleFor), spent_[from]);
        spent_[from] -= move;
        spent_[to] += move;
        categoryBeforeAway_ = lastCategory_;
        lastCategory_       = ActivityCategory::Away;
        refreshSummarySeconds();
    }
    // Later samples need no move: creditElapsed() already credits Away while lastCategory_ is Away.
}

void SessionManager::logManualAiUse(Clock::time_point /*now*/) {
    if (!isActive()) return;
    ++summary_.aiManualLogs;
    notifyListener(SessionEvent::Type::AiVisit);
    checkAiBudget();
}

void SessionManager::creditElapsed(Clock::time_point now) {
    const auto delta = std::max(now - lastSampleAt_, Clock::duration::zero());
    spent_[static_cast<std::size_t>(lastCategory_)] += delta;
    refreshSummarySeconds();
    lastSampleAt_ = now;
}

void SessionManager::refreshSummarySeconds() {
    for (std::size_t i = 0; i < spent_.size(); ++i) {
        summary_.secondsByCategory[i] = std::chrono::duration_cast<std::chrono::seconds>(spent_[i]).count();
    }
}

void SessionManager::checkAiBudget() {
    if (summary_.aiBudget && summary_.aiTotal() > *summary_.aiBudget && !budgetExceededEmitted_) {
        budgetExceededEmitted_ = true;  // warn once, not on every later visit
        notifyListener(SessionEvent::Type::AiBudgetExceeded);
    }
}

void SessionManager::finish(SessionOutcome outcome, Clock::time_point now) {
    auto active = timer_.elapsed(now).count();
    if (outcome == SessionOutcome::Completed) active = std::min(active, summary_.plannedSeconds);
    summary_.activeSeconds = active;
    summary_.outcome       = outcome;
    summary_.endedAt       = unixNow();

    timer_.stop();
    state_ = SessionState::Idle;  // summary_ stays readable until the next start()
    notifyListener(outcome == SessionOutcome::Completed ? SessionEvent::Type::Completed
                                              : SessionEvent::Type::Abandoned);
}

void SessionManager::notifyListener(SessionEvent::Type type) const {
    if (listener_) listener_(SessionEvent{type});
}

}  // namespace focus
