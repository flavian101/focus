#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace focus {

// How much a task matters today. Lower number = more important.
enum class Priority { Must = 1, Should = 2, Could = 3 };

// "Dropped" means "I decided not to do this today". It is kept (not deleted) so that
// planned-versus-done stays honest.
enum class TaskStatus { Todo, Done, Dropped };

constexpr std::string_view statusName(TaskStatus s) {
    switch (s) {
        case TaskStatus::Todo:    return "todo";
        case TaskStatus::Done:    return "done";
        case TaskStatus::Dropped: return "dropped";
    }
    return "todo";
}

inline std::optional<TaskStatus> parseTaskStatus(std::string_view s) {
    if (s == "todo")    return TaskStatus::Todo;
    if (s == "done")    return TaskStatus::Done;
    if (s == "dropped") return TaskStatus::Dropped;
    return std::nullopt;
}

constexpr std::string_view priorityLabel(Priority p) {
    switch (p) {
        case Priority::Must:   return "Must";
        case Priority::Should: return "Should";
        case Priority::Could:  return "Could";
    }
    return "Should";
}

// One thing you intend to do on a particular day.
struct Task {
    std::int64_t id = 0;  // 0 = not saved yet
    std::string title;
    Priority priority = Priority::Should;
    int estimateMinutes = 25;
    TaskStatus status = TaskStatus::Todo;
    std::string day;  // local calendar day "YYYY-MM-DD". The caller decides what "today" is, so core never needs a time zone.
    std::int64_t createdAt = 0;    // unix seconds
    std::int64_t completedAt = 0;  // unix seconds, 0 while not done
    std::int64_t actualSeconds = 0;  // time spent in sessions linked to this task (filled in by storage)
};

}  // namespace focus
