#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/Task.h"
#include "storage/Database.h"

namespace focus {

class TaskRepository {
public:
    explicit TaskRepository(Database& db) : db_(db) {}

    std::int64_t insert(const Task& task);  // returns the new row id
    std::optional<Task> find(std::int64_t id);

    // One day's tasks, most important first, then in the order they were added.
    // Task::actualSeconds is the total time of sessions linked to each task.
    std::vector<Task> forDay(const std::string& day);

    // Tasks still "todo" on days BEFORE `day` (ISO dates sort correctly as text), oldest first.
    std::vector<Task> unfinishedBefore(const std::string& day);

    void setStatus(std::int64_t id, TaskStatus status, std::int64_t completedAt);
    void setPriority(std::int64_t id, Priority priority);
    void moveToDay(std::int64_t id, const std::string& day);

private:
    Database& db_;
};

}  // namespace focus
