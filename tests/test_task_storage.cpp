#include <doctest/doctest.h>

#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/SessionRepository.h"
#include "storage/TaskRepository.h"

using namespace focus;

namespace {
Database freshDb() {
    auto db = Database::open(":memory:");
    migrate(db);
    return db;
}

Task newTask(const char* title, Priority p, const char* day, int minutes = 30) {
    Task t;
    t.title           = title;
    t.priority        = p;
    t.estimateMinutes = minutes;
    t.day             = day;
    t.createdAt       = 100;
    return t;
}
}  // namespace

TEST_CASE("a task round-trips through storage") {
    auto db = freshDb();
    TaskRepository repo(db);
    const auto id = repo.insert(newTask("write the plan screen", Priority::Must, "2026-11-23", 45));
    auto found    = repo.find(id);
    REQUIRE(found.has_value());
    CHECK(found->title == "write the plan screen");
    CHECK(found->priority == Priority::Must);
    CHECK(found->estimateMinutes == 45);
    CHECK(found->status == TaskStatus::Todo);
    CHECK(found->day == "2026-11-23");
    CHECK(found->completedAt == 0);
    CHECK_FALSE(repo.find(9999).has_value());
}

TEST_CASE("a day's tasks come back most important first, then in the order added") {
    auto db = freshDb();
    TaskRepository repo(db);
    repo.insert(newTask("could A", Priority::Could, "2026-11-23"));
    repo.insert(newTask("must A", Priority::Must, "2026-11-23"));
    repo.insert(newTask("should A", Priority::Should, "2026-11-23"));
    repo.insert(newTask("must B", Priority::Must, "2026-11-23"));
    repo.insert(newTask("other day", Priority::Must, "2026-11-24"));

    auto day = repo.forDay("2026-11-23");
    REQUIRE(day.size() == 4);
    CHECK(day[0].title == "must A");
    CHECK(day[1].title == "must B");
    CHECK(day[2].title == "should A");
    CHECK(day[3].title == "could A");
}

TEST_CASE("status and priority changes are saved; done records when") {
    auto db = freshDb();
    TaskRepository repo(db);
    const auto id = repo.insert(newTask("x", Priority::Should, "2026-11-23"));

    repo.setStatus(id, TaskStatus::Done, 5000);
    CHECK(repo.find(id)->status == TaskStatus::Done);
    CHECK(repo.find(id)->completedAt == 5000);

    repo.setStatus(id, TaskStatus::Todo, 0);  // un-checking clears the time
    CHECK(repo.find(id)->completedAt == 0);

    repo.setPriority(id, Priority::Could);
    CHECK(repo.find(id)->priority == Priority::Could);
}

TEST_CASE("time spent comes from the sessions linked to a task") {
    auto db = freshDb();
    TaskRepository tasks(db);
    SessionRepository sessions(db);
    const auto id    = tasks.insert(newTask("linked", Priority::Must, "2026-11-23"));
    const auto other = tasks.insert(newTask("unrelated", Priority::Must, "2026-11-23"));

    for (int i = 0; i < 2; ++i) {
        SessionSummary s;
        s.goal           = "work";
        s.plannedSeconds = 1500;
        s.activeSeconds  = 1200;
        s.taskId         = id;
        sessions.insert(s);
    }
    SessionSummary loose;  // a session with no task at all
    loose.goal           = "no task";
    loose.plannedSeconds = 600;
    loose.activeSeconds  = 600;
    sessions.insert(loose);

    CHECK(tasks.find(id)->actualSeconds == 2400);
    CHECK(tasks.find(other)->actualSeconds == 0);
    auto recent = sessions.recent(5);
    CHECK(recent.size() == 3);
    int linked = 0;
    for (const auto& r : recent) linked += r.taskId.has_value() && *r.taskId == id;
    CHECK(linked == 2);
}

TEST_CASE("unfinished tasks from earlier days can be found and moved") {
    auto db = freshDb();
    TaskRepository repo(db);
    const auto old1 = repo.insert(newTask("old todo", Priority::Should, "2026-11-20"));
    const auto old2 = repo.insert(newTask("old done", Priority::Should, "2026-11-21"));
    repo.setStatus(old2, TaskStatus::Done, 1);
    repo.insert(newTask("today", Priority::Must, "2026-11-23"));

    auto stale = repo.unfinishedBefore("2026-11-23");
    REQUIRE(stale.size() == 1);
    CHECK(stale[0].id == old1);

    repo.moveToDay(old1, "2026-11-23");
    CHECK(repo.unfinishedBefore("2026-11-23").empty());
    CHECK(repo.forDay("2026-11-23").size() == 2);
}

TEST_CASE("the database refuses nonsense") {
    auto db = freshDb();
    TaskRepository repo(db);
    auto bad = newTask("bad", Priority::Must, "2026-11-23");
    bad.estimateMinutes = 0;
    CHECK_THROWS_AS(repo.insert(bad), StorageError);
}

TEST_CASE("upgrading a v2 database adds the plan tables and keeps sessions") {
    auto db = Database::open(":memory:");
    migrate(db, 2);
    db.exec("INSERT INTO sessions (goal, planned_s, active_s, started_at, ended_at, outcome)"
            " VALUES ('before the plan existed', 60, 60, 1, 2, 'completed');");
    CHECK(db.userVersion() == 2);

    migrate(db);
    CHECK(db.userVersion() == 7);
    auto rows = SessionRepository(db).recent(5);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].goal == "before the plan existed");
    CHECK_FALSE(rows[0].taskId.has_value());
    CHECK_NOTHROW(TaskRepository(db).insert(newTask("works", Priority::Could, "2026-11-23")));
}
