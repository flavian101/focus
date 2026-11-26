#include <doctest/doctest.h>

#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/SessionRepository.h"
#include "storage/SettingsRepository.h"
#include "storage/TaskRepository.h"
#include "storage/WellnessRepository.h"

using namespace focus;

namespace {
Database freshDb() {
    auto db = Database::open(":memory:");
    migrate(db);
    return db;
}
}  // namespace

TEST_CASE("water: glasses add up per day and remember the latest time") {
    auto db = freshDb();
    WellnessRepository repo(db);
    CHECK(repo.waterFor("2026-11-23").glasses == 0);
    CHECK(repo.waterFor("2026-11-23").lastGlassMinute == -1);

    repo.addGlass("2026-11-23", 9 * 60 + 5, 100);
    repo.addGlass("2026-11-23", 11 * 60 + 40, 200);
    repo.addGlass("2026-11-24", 8 * 60, 300);  // another day

    auto today = repo.waterFor("2026-11-23");
    CHECK(today.glasses == 2);
    CHECK(today.lastGlassMinute == 11 * 60 + 40);
    CHECK(repo.waterFor("2026-11-24").glasses == 1);
}

TEST_CASE("water: undo removes only the most recent glass of that day") {
    auto db = freshDb();
    WellnessRepository repo(db);
    repo.addGlass("2026-11-23", 540, 1);
    repo.addGlass("2026-11-23", 600, 2);
    repo.addGlass("2026-11-24", 480, 3);

    CHECK(repo.undoLastGlass("2026-11-23"));
    auto day = repo.waterFor("2026-11-23");
    CHECK(day.glasses == 1);
    CHECK(day.lastGlassMinute == 540);  // the earlier glass is now the latest
    CHECK(repo.waterFor("2026-11-24").glasses == 1);

    CHECK(repo.undoLastGlass("2026-11-23"));
    CHECK_FALSE(repo.undoLastGlass("2026-11-23"));  // nothing left to undo
}

TEST_CASE("meals: one answer per meal per day, and it can be changed or cleared") {
    auto db = freshDb();
    WellnessRepository repo(db);
    repo.setMeal("2026-11-23", "lunch", MealStatus::Skipped, 1);
    repo.setMeal("2026-11-23", "lunch", MealStatus::Ate, 2);  // change of mind replaces it
    repo.setMeal("2026-11-23", "dinner", MealStatus::Ate, 3);
    repo.setMeal("2026-11-24", "lunch", MealStatus::Skipped, 4);

    auto meals = repo.mealsFor("2026-11-23");
    REQUIRE(meals.size() == 2);
    CHECK(meals.at("lunch") == MealStatus::Ate);

    repo.clearMeal("2026-11-23", "lunch");
    CHECK(repo.mealsFor("2026-11-23").size() == 1);
    CHECK(repo.mealsFor("2026-11-24").size() == 1);
}

TEST_CASE("settings: values persist, fall back when missing or not a number") {
    auto db = freshDb();
    SettingsRepository settings(db);
    CHECK_FALSE(settings.get("water_goal").has_value());
    CHECK(settings.getInt("water_goal", 8) == 8);

    settings.setInt("water_goal", 10);
    CHECK(settings.getInt("water_goal", 8) == 10);
    settings.setInt("water_goal", 6);  // overwrite
    CHECK(settings.getInt("water_goal", 8) == 6);

    settings.set("water_goal", "lots");
    CHECK(settings.getInt("water_goal", 8) == 8);  // garbage falls back instead of throwing
}

TEST_CASE("the database refuses a glass at an impossible time") {
    auto db = freshDb();
    WellnessRepository repo(db);
    CHECK_THROWS_AS(repo.addGlass("2026-11-23", 2000, 1), StorageError);
}

TEST_CASE("upgrading a v3 database adds water, meals and settings and keeps tasks and sessions") {
    auto db = Database::open(":memory:");
    migrate(db, 3);
    Task t;
    t.title = "survived the upgrade"; t.day = "2026-11-23"; t.createdAt = 1;
    TaskRepository(db).insert(t);
    CHECK(db.userVersion() == 3);

    migrate(db);
    CHECK(db.userVersion() == 7);
    CHECK(TaskRepository(db).forDay("2026-11-23").size() == 1);
    CHECK_NOTHROW(WellnessRepository(db).addGlass("2026-11-23", 600, 1));
    CHECK_NOTHROW(SettingsRepository(db).set("k", "v"));
}
