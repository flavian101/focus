#include "storage/TaskRepository.h"

namespace focus {

namespace {

// actual_s is computed on the fly from linked sessions, so it can never drift out of sync.
constexpr const char* kSelect =
    "SELECT t.id, t.title, t.priority, t.estimate_min, t.status, t.day, t.created_at, t.completed_at,"
    " COALESCE((SELECT SUM(s.active_s) FROM sessions s WHERE s.task_id = t.id), 0) AS actual_s"
    " FROM tasks t";

Task readTask(Statement& st) {
    Task t;
    t.id              = st.columnInt(0);
    t.title           = st.columnText(1);
    t.priority        = static_cast<Priority>(st.columnInt(2));
    t.estimateMinutes = static_cast<int>(st.columnInt(3));
    t.status          = parseTaskStatus(st.columnText(4)).value_or(TaskStatus::Todo);
    t.day             = st.columnText(5);
    t.createdAt       = st.columnInt(6);
    t.completedAt     = st.columnIsNull(7) ? 0 : st.columnInt(7);
    t.actualSeconds   = st.columnInt(8);
    return t;
}

}  // namespace

std::int64_t TaskRepository::insert(const Task& t) {
    auto st = db_.prepare(
        "INSERT INTO tasks (title, priority, estimate_min, status, day, created_at, completed_at)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);");
    st.bind(1, t.title)
        .bind(2, static_cast<std::int64_t>(t.priority))
        .bind(3, t.estimateMinutes)
        .bind(4, std::string(statusName(t.status)))
        .bind(5, t.day)
        .bind(6, t.createdAt);
    if (t.completedAt != 0) st.bind(7, t.completedAt); else st.bindNull(7);
    st.run();
    return db_.lastInsertRowId();
}

std::optional<Task> TaskRepository::find(std::int64_t id) {
    auto st = db_.prepare(std::string(kSelect) + " WHERE t.id = ?1;");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readTask(st);
}

std::vector<Task> TaskRepository::forDay(const std::string& day) {
    auto st = db_.prepare(std::string(kSelect) + " WHERE t.day = ?1 ORDER BY t.priority, t.id;");
    st.bind(1, day);
    std::vector<Task> out;
    while (st.step()) out.push_back(readTask(st));
    return out;
}

std::vector<Task> TaskRepository::unfinishedBefore(const std::string& day) {
    auto st = db_.prepare(std::string(kSelect) + " WHERE t.status = 'todo' AND t.day < ?1 ORDER BY t.day, t.priority, t.id;");
    st.bind(1, day);
    std::vector<Task> out;
    while (st.step()) out.push_back(readTask(st));
    return out;
}

void TaskRepository::setStatus(std::int64_t id, TaskStatus status, std::int64_t completedAt) {
    auto st = db_.prepare("UPDATE tasks SET status = ?1, completed_at = ?2 WHERE id = ?3;");
    st.bind(1, std::string(statusName(status)));
    if (status == TaskStatus::Done && completedAt != 0) st.bind(2, completedAt); else st.bindNull(2);
    st.bind(3, id);
    st.run();
}

void TaskRepository::setPriority(std::int64_t id, Priority priority) {
    auto st = db_.prepare("UPDATE tasks SET priority = ?1 WHERE id = ?2;");
    st.bind(1, static_cast<std::int64_t>(priority)).bind(2, id);
    st.run();
}

void TaskRepository::moveToDay(std::int64_t id, const std::string& day) {
    auto st = db_.prepare("UPDATE tasks SET day = ?1 WHERE id = ?2;");
    st.bind(1, day).bind(2, id);
    st.run();
}

}  // namespace focus
