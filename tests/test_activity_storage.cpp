#include <doctest/doctest.h>

#include "storage/ActivityRepository.h"
#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/TaskRepository.h"

using namespace focus;
using Cat = ActivityCategory;

namespace {
Database freshDb() {
    auto db = Database::open(":memory:");
    migrate(db);
    return db;
}

ActivityBucket bucket(const char* day, int minute, Cat c, const char* app, int seconds) {
    return ActivityBucket{day, minute, c, app, seconds};
}
}  // namespace

TEST_CASE("activity: buckets round-trip, ordered by minute") {
    auto db = freshDb();
    ActivityRepository repo(db);
    repo.addSeconds({bucket("2026-11-23", 601, Cat::Focus, "code", 40),
                     bucket("2026-11-23", 600, Cat::AiAssistant, "chrome", 20),
                     bucket("2026-11-24", 600, Cat::Neutral, "slack", 10)});

    auto day = repo.forDay("2026-11-23");
    REQUIRE(day.size() == 2);
    CHECK(day[0].minuteOfDay == 600);
    CHECK(day[0].category == Cat::AiAssistant);
    CHECK(day[0].app == "chrome");
    CHECK(day[1].seconds == 40);
    CHECK(repo.forDay("2026-11-24").size() == 1);
}

TEST_CASE("activity: saving the same minute again adds to it instead of duplicating") {
    auto db = freshDb();
    ActivityRepository repo(db);
    repo.addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 30)});
    repo.addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 25), bucket("2026-11-23", 600, Cat::Focus, "devenv", 5)});

    auto day = repo.forDay("2026-11-23");
    REQUIRE(day.size() == 2);                       // code and devenv
    int code = 0;
    for (const auto& b : day) if (b.app == "code") code = b.seconds;
    CHECK(code == 55);
}

TEST_CASE("activity: a batch with a bad row is saved completely or not at all") {
    auto db = freshDb();
    ActivityRepository repo(db);
    // minute 5000 is outside 0..1439, which the database refuses
    CHECK_THROWS_AS(repo.addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 30), bucket("2026-11-23", 5000, Cat::Focus, "code", 30)}),
                    StorageError);
    CHECK(repo.forDay("2026-11-23").empty());       // the good row was rolled back too
}

TEST_CASE("activity: clearing one day leaves the others") {
    auto db = freshDb();
    ActivityRepository repo(db);
    repo.addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 30), bucket("2026-11-24", 600, Cat::Focus, "code", 30)});
    repo.clearDay("2026-11-23");
    CHECK(repo.forDay("2026-11-23").empty());
    CHECK(repo.forDay("2026-11-24").size() == 1);
    repo.clearAll();
    CHECK(repo.forDay("2026-11-24").empty());
}

TEST_CASE("activity: nothing is stored for empty or zero-second buckets") {
    auto db = freshDb();
    ActivityRepository repo(db);
    CHECK_NOTHROW(repo.addSeconds({}));
    CHECK_NOTHROW(repo.addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 0)}));
    CHECK(repo.forDay("2026-11-23").empty());
}

TEST_CASE("upgrading a v4 database adds the activity table and keeps tasks") {
    auto db = Database::open(":memory:");
    migrate(db, 4);
    Task t;
    t.title = "survived"; t.day = "2026-11-23"; t.createdAt = 1;
    TaskRepository(db).insert(t);
    CHECK(db.userVersion() == 4);

    migrate(db);
    CHECK(db.userVersion() == 5);
    CHECK(TaskRepository(db).forDay("2026-11-23").size() == 1);
    CHECK_NOTHROW(ActivityRepository(db).addSeconds({bucket("2026-11-23", 600, Cat::Focus, "code", 30)}));
}
