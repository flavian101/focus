#include "app/Report.h"

#include <cstdio>
#include <iostream>

#include "app/TimeUtil.h"

namespace focus {

namespace {
std::string oneDecimal(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}
}  // namespace

void printSessionSummary(const SessionSummary& s) {
    std::cout << "\n--- Session " << (s.outcome == SessionOutcome::Completed ? "complete" : "abandoned") << " ---\n"
              << "Goal:       " << s.goal << "\n"
              << "Focused:    " << formatDuration(s.activeSeconds) << " of " << formatDuration(s.plannedSeconds) << "\n"
              << "Time in:    focus " << formatDuration(s.secondsIn(ActivityCategory::Focus))
              << ", other " << formatDuration(s.secondsIn(ActivityCategory::Neutral))
              << ", distraction " << formatDuration(s.secondsIn(ActivityCategory::Distraction))
              << ", AI " << formatDuration(s.secondsIn(ActivityCategory::AiAssistant))
              << ", away " << formatDuration(s.secondsIn(ActivityCategory::Away)) << "\n"
              << "AI visits:  " << s.aiTotal();
    if (s.aiBudget) std::cout << " (budget " << *s.aiBudget << ")";
    std::cout << "\n";
}

void printReport(const StatsReport& r, int days) {
    std::cout << "\nLast " << days << " days\n"
              << "  Sessions:    " << r.sessionsCompleted << " completed, " << r.sessionsAbandoned << " abandoned\n"
              << "  Focus time:  " << formatDuration(r.focusSeconds) << " (away from keyboard: "
              << formatDuration(r.awaySeconds) << ")\n"
              << "  AI use:      " << oneDecimal(r.aiVisitsPerSession) << " visits/session, "
              << oneDecimal(r.aiTimeShare * 100.0) << "% of tracked time, " << r.sessionsOverAiBudget
              << " session(s) over budget\n";

    if (r.avgMoodBefore && r.avgMoodAfter) {
        std::cout << "  Mood:        " << oneDecimal(*r.avgMoodBefore) << " before -> " << oneDecimal(*r.avgMoodAfter)
                  << " after";
        if (r.avgMoodChange) std::cout << " (avg change " << (*r.avgMoodChange >= 0 ? "+" : "") << oneDecimal(*r.avgMoodChange) << ")";
        std::cout << "\n";
    }
    if (!r.topTags.empty()) {
        std::cout << "  Feelings:    ";
        for (std::size_t i = 0; i < r.topTags.size(); ++i) {
            std::cout << (i ? ", " : "") << r.topTags[i].first << " (" << r.topTags[i].second << ")";
        }
        std::cout << "\n";
    }
}

}  // namespace focus
