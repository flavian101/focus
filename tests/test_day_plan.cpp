#include <doctest/doctest.h>

#include "core/DayPlan.h"

using namespace focus;

namespace {
Task task(std::int64_t id, Priority p, int minutes, TaskStatus status = TaskStatus::Todo, std::int64_t actualSeconds = 0) {
    Task t;
    t.id              = id;
    t.title           = "task " + std::to_string(id);
    t.priority        = p;
    t.estimateMinutes = minutes;
    t.status          = status;
    t.actualSeconds   = actualSeconds;
    return t;
}
const PlanRules rules;  // defaults: 3 musts, 240 minutes
}  // namespace

TEST_CASE("a task needs a name and a sane estimate") {
    Task t = task(0, Priority::Should, 25);
    CHECK(checkTask({}, t, rules) == PlanCheck::Ok);

    t.title = "   ";
    CHECK(checkTask({}, t, rules) == PlanCheck::EmptyTitle);
    t.title = std::string(121, 'x');
    CHECK(checkTask({}, t, rules) == PlanCheck::TitleTooLong);

    t = task(0, Priority::Should, 0);
    CHECK(checkTask({}, t, rules) == PlanCheck::BadEstimate);
    t.estimateMinutes = 481;
    CHECK(checkTask({}, t, rules) == PlanCheck::BadEstimate);
}

TEST_CASE("only three must-dos fit in a day") {
    std::vector<Task> day = {task(1, Priority::Must, 30), task(2, Priority::Must, 30), task(3, Priority::Must, 30)};
    CHECK(checkTask(day, task(0, Priority::Must, 30), rules) == PlanCheck::TooManyMust);
    CHECK(checkTask(day, task(0, Priority::Should, 30), rules) == PlanCheck::Ok);  // other priorities are not limited
    CHECK(describe(PlanCheck::TooManyMust, rules).find("3 must-dos") != std::string::npos);
}

TEST_CASE("dropped must-dos free a slot, and a task is not counted against itself") {
    std::vector<Task> day = {task(1, Priority::Must, 30), task(2, Priority::Must, 30),
                             task(3, Priority::Must, 30, TaskStatus::Dropped)};
    CHECK(checkTask(day, task(0, Priority::Must, 30), rules) == PlanCheck::Ok);  // only two live musts

    day.push_back(task(4, Priority::Must, 30));  // now three live musts
    CHECK(checkTask(day, task(0, Priority::Must, 30), rules) == PlanCheck::TooManyMust);
    // Editing must-do #4 (keeping it a must) is fine: it is not blocked by itself.
    CHECK(checkTask(day, task(4, Priority::Must, 45), rules) == PlanCheck::Ok);
    // Promoting a Should to Must is blocked, because three live musts already exist.
    CHECK(checkTask(day, task(9, Priority::Must, 30), rules) == PlanCheck::TooManyMust);
}

TEST_CASE("summary counts committed work and ignores dropped tasks") {
    std::vector<Task> day = {
        task(1, Priority::Must, 60, TaskStatus::Done, 3000),
        task(2, Priority::Must, 30),
        task(3, Priority::Should, 45, TaskStatus::Done, 2400),
        task(4, Priority::Could, 20, TaskStatus::Dropped, 600),
    };
    const auto s = summarizePlan(day, rules);
    CHECK(s.total == 3);
    CHECK(s.done == 2);
    CHECK(s.dropped == 1);
    CHECK(s.mustTotal == 2);
    CHECK(s.mustDone == 1);
    CHECK(s.plannedMinutes == 135);   // 60 + 30 + 45
    CHECK(s.doneMinutes == 105);      // 60 + 45
    CHECK(s.actualMinutes == 100);    // (3000 + 2400 + 600) / 60: time on dropped work still counts as time spent
    CHECK(s.completion == doctest::Approx(2.0 / 3.0));
    CHECK_FALSE(s.overcommitted);
}

TEST_CASE("planning more than a day can hold is flagged") {
    std::vector<Task> day = {task(1, Priority::Must, 200), task(2, Priority::Should, 60)};
    CHECK(summarizePlan(day, rules).overcommitted);          // 260 > 240
    day[1].status = TaskStatus::Dropped;
    CHECK_FALSE(summarizePlan(day, rules).overcommitted);    // dropping it brings the plan back in range
}

TEST_CASE("an empty day has an empty summary") {
    const auto s = summarizePlan({}, rules);
    CHECK(s.total == 0);
    CHECK(s.completion == 0.0);
    CHECK_FALSE(s.overcommitted);
}
