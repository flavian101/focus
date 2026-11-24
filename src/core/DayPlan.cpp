#include "core/DayPlan.h"

#include <algorithm>

namespace focus {

namespace {
constexpr std::size_t kMaxTitleLength = 120;
constexpr int kMaxEstimate            = 480;  // 8 hours

bool blank(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; });
}
}  // namespace

PlanCheck checkTask(const std::vector<Task>& dayTasks, const Task& candidate, const PlanRules& rules) {
    if (blank(candidate.title)) return PlanCheck::EmptyTitle;
    if (candidate.title.size() > kMaxTitleLength) return PlanCheck::TitleTooLong;
    if (candidate.estimateMinutes < 1 || candidate.estimateMinutes > kMaxEstimate) return PlanCheck::BadEstimate;

    if (candidate.priority == Priority::Must && candidate.status != TaskStatus::Dropped) {
        const auto musts = std::count_if(dayTasks.begin(), dayTasks.end(), [&](const Task& t) {
            return t.id != candidate.id && t.priority == Priority::Must && t.status != TaskStatus::Dropped;
        });
        if (musts >= rules.maxMust) return PlanCheck::TooManyMust;
    }
    return PlanCheck::Ok;
}

std::string describe(PlanCheck check, const PlanRules& rules) {
    switch (check) {
        case PlanCheck::Ok:           return {};
        case PlanCheck::EmptyTitle:   return "Give the task a name.";
        case PlanCheck::TitleTooLong: return "Keep the name under " + std::to_string(kMaxTitleLength) + " characters.";
        case PlanCheck::BadEstimate:  return "Estimate between 1 and " + std::to_string(kMaxEstimate) + " minutes.";
        case PlanCheck::TooManyMust:
            return "You already have " + std::to_string(rules.maxMust) +
                   " must-dos today. Make one a Should or drop it first. If everything is a must, nothing is.";
    }
    return {};
}

PlanSummary summarizePlan(const std::vector<Task>& dayTasks, const PlanRules& rules) {
    PlanSummary s;
    s.capacityMinutes = rules.capacityMinutes;
    std::int64_t actualSeconds = 0;
    for (const auto& t : dayTasks) {
        actualSeconds += t.actualSeconds;
        if (t.status == TaskStatus::Dropped) {
            ++s.dropped;
            continue;
        }
        ++s.total;
        s.plannedMinutes += t.estimateMinutes;
        if (t.priority == Priority::Must) ++s.mustTotal;
        if (t.status == TaskStatus::Done) {
            ++s.done;
            s.doneMinutes += t.estimateMinutes;
            if (t.priority == Priority::Must) ++s.mustDone;
        }
    }
    s.actualMinutes = static_cast<int>(actualSeconds / 60);
    s.overcommitted = s.plannedMinutes > rules.capacityMinutes;
    s.completion    = s.total > 0 ? static_cast<double>(s.done) / s.total : 0.0;
    return s;
}

}  // namespace focus
