#include "core/Statistics.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>

namespace focus {

namespace {

std::optional<double> average(const std::vector<int>& values) {
    if (values.empty()) return std::nullopt;
    double sum = 0;
    for (int v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

std::string lowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

StatsReport summarize(const std::vector<SessionSummary>& sessions, const std::vector<CheckIn>& checkIns) {
    StatsReport r;

    std::int64_t aiSeconds = 0, trackedSeconds = 0;
    std::int64_t aiVisits = 0;
    for (const auto& s : sessions) {
        (s.outcome == SessionOutcome::Completed ? r.sessionsCompleted : r.sessionsAbandoned)++;
        r.focusSeconds += s.activeSeconds;
        aiVisits += s.aiTotal();
        if (s.aiBudget && s.aiTotal() > *s.aiBudget) ++r.sessionsOverAiBudget;
        for (std::size_t i = 0; i < s.secondsByCategory.size(); ++i) {
            if (i == static_cast<std::size_t>(ActivityCategory::Away)) continue;  // not present: not in the share
            trackedSeconds += s.secondsByCategory[i];
        }
        r.awaySeconds += s.secondsIn(ActivityCategory::Away);
        aiSeconds += s.secondsIn(ActivityCategory::AiAssistant);
    }
    if (!sessions.empty()) r.aiVisitsPerSession = static_cast<double>(aiVisits) / sessions.size();
    if (trackedSeconds > 0) r.aiTimeShare = static_cast<double>(aiSeconds) / trackedSeconds;

    // Pair up before/after check-ins per session; later check-ins overwrite earlier ones.
    struct Pair { std::optional<int> before, after; };
    std::unordered_map<std::int64_t, Pair> bySession;
    std::map<std::string, int> tagCounts;
    for (const auto& c : checkIns) {
        if (!c.isValid()) continue;
        if (c.sessionId) {
            auto& p = bySession[*c.sessionId];
            if (c.kind == CheckInKind::BeforeSession) p.before = c.mood;
            if (c.kind == CheckInKind::AfterSession)  p.after = c.mood;
        }
        if (!c.tag.empty()) ++tagCounts[lowerAscii(c.tag)];
    }

    std::vector<int> befores, afters, changes;
    for (const auto& [id, p] : bySession) {
        if (p.before) befores.push_back(*p.before);
        if (p.after) afters.push_back(*p.after);
        if (p.before && p.after) changes.push_back(*p.after - *p.before);
    }
    r.avgMoodBefore = average(befores);
    r.avgMoodAfter  = average(afters);
    r.avgMoodChange = average(changes);

    r.topTags.assign(tagCounts.begin(), tagCounts.end());
    std::stable_sort(r.topTags.begin(), r.topTags.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    if (r.topTags.size() > 3) r.topTags.resize(3);
    return r;
}

}  // namespace focus
