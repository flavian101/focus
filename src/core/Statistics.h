#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/CheckIn.h"
#include "core/SessionManager.h"

namespace focus {

struct StatsReport {
    int sessionsCompleted = 0;
    int sessionsAbandoned = 0;
    std::int64_t focusSeconds = 0;       // sum of active time across sessions
    double aiVisitsPerSession = 0.0;
    int sessionsOverAiBudget = 0;
    double aiTimeShare = 0.0;            // 0..1: fraction of tracked time spent in AI tools

    std::optional<double> avgMoodBefore;
    std::optional<double> avgMoodAfter;
    std::optional<double> avgMoodChange; // only over sessions that have BOTH a before and after check-in
    std::vector<std::pair<std::string, int>> topTags;  // most frequent mood labels, up to 3
};

// Pure function: data in, numbers out. No database, no clock.
StatsReport summarize(const std::vector<SessionSummary>& sessions, const std::vector<CheckIn>& checkIns);

}  // namespace focus
