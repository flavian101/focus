#pragma once
#include <string>
#include <vector>

#include "core/Task.h"

namespace focus {

// The rules of a good plan. The point of the limit is to force the choice:
// if everything is a "must", nothing is.
struct PlanRules {
    int maxMust         = 3;    // most "Must" tasks allowed on one day
    int capacityMinutes = 240;  // how much focused work you can realistically do in a day
};

enum class PlanCheck { Ok, EmptyTitle, TitleTooLong, BadEstimate, TooManyMust };

// Is `candidate` allowed on a day that already has `dayTasks`?
// If candidate.id is not 0 it is an existing task being changed, so it is not counted against itself.
PlanCheck checkTask(const std::vector<Task>& dayTasks, const Task& candidate, const PlanRules& rules);

// A sentence you can show the user. Empty for PlanCheck::Ok.
std::string describe(PlanCheck check, const PlanRules& rules);

struct PlanSummary {
    int total   = 0;  // tasks you are still committed to (everything except dropped)
    int done    = 0;
    int dropped = 0;
    int mustTotal = 0;
    int mustDone  = 0;
    int plannedMinutes  = 0;  // estimates of committed tasks
    int doneMinutes     = 0;  // estimates of finished tasks
    int actualMinutes   = 0;  // time really spent in linked sessions
    int capacityMinutes = 0;
    bool overcommitted  = false;  // planned more than a day can hold
    double completion   = 0.0;    // 0..1, done / total
};

PlanSummary summarizePlan(const std::vector<Task>& dayTasks, const PlanRules& rules);

}  // namespace focus
