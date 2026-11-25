#include <doctest/doctest.h>

#include "storage/ActivityRepository.h"
#include "storage/AppRuleRepository.h"
#include "storage/Database.h"
#include "storage/Migrations.h"

using namespace focus;
using Cat = ActivityCategory;

namespace {
Database freshDb() {
    auto db = Database::open(":memory:");
    migrate(db);
    return db;
}
}  // namespace

TEST_CASE("app rules: saved, replaced and removed") {
    auto db = freshDb();
    AppRuleRepository rules(db);
    CHECK(rules.all().empty());

    rules.set("figma", Cat::Focus, 1);
    rules.set("steam", Cat::Distraction, 2);
    rules.set("figma", Cat::Neutral, 3);  // changing your mind replaces the rule
    auto all = rules.all();
    REQUIRE(all.size() == 2);
    CHECK(all.at("figma") == Cat::Neutral);
    CHECK(all.at("steam") == Cat::Distraction);

    rules.remove("steam");
    CHECK(rules.all().size() == 1);
}

TEST_CASE("app rules: 'away' is not something an app can be") {
    auto db = freshDb();
    CHECK_THROWS_AS(AppRuleRepository(db).set("figma", Cat::Away, 1), StorageError);
}

TEST_CASE("recategorize: saved time moves to the new category and merges with existing time") {
    auto db = freshDb();
    ActivityRepository repo(db);
    repo.addSeconds({{"2026-11-23", 600, Cat::Neutral, "figma", 30},
                     {"2026-11-23", 600, Cat::Focus, "figma", 20},     // the same minute already has focus time
                     {"2026-11-23", 601, Cat::Neutral, "figma", 60},
                     {"2026-11-23", 601, Cat::Neutral, "slack", 60},   // another app: untouched
                     {"2026-11-24", 600, Cat::Neutral, "figma", 60}}); // another day: untouched

    repo.recategorize("2026-11-23", "figma", std::nullopt, Cat::Focus);
    auto day = repo.forDay("2026-11-23");
    int focusMinute600 = 0, focusMinute601 = 0, slack = 0, neutralFigma = 0;
    for (const auto& b : day) {
        if (b.app == "figma" && b.category == Cat::Focus && b.minuteOfDay == 600) focusMinute600 = b.seconds;
        if (b.app == "figma" && b.category == Cat::Focus && b.minuteOfDay == 601) focusMinute601 = b.seconds;
        if (b.app == "figma" && b.category == Cat::Neutral) neutralFigma += b.seconds;
        if (b.app == "slack") slack = b.seconds;
    }
    CHECK(focusMinute600 == 50);   // 20 existing + 30 moved, merged into one row
    CHECK(focusMinute601 == 60);
    CHECK(neutralFigma == 0);
    CHECK(slack == 60);
    CHECK(repo.forDay("2026-11-24")[0].category == Cat::Neutral);
}

TEST_CASE("recategorize: a browser's AI time is left alone when only neutral time is moved") {
    auto db = freshDb();
    ActivityRepository repo(db);
    repo.addSeconds({{"2026-11-23", 600, Cat::Neutral, "chrome", 40}, {"2026-11-23", 601, Cat::AiAssistant, "chrome", 60}});
    repo.recategorize("2026-11-23", "chrome", Cat::Neutral, Cat::Focus);
    int focus = 0, ai = 0;
    for (const auto& b : repo.forDay("2026-11-23")) {
        if (b.category == Cat::Focus) focus += b.seconds;
        if (b.category == Cat::AiAssistant) ai += b.seconds;
    }
    CHECK(focus == 40);
    CHECK(ai == 60);
}

TEST_CASE("upgrading a v5 database adds app rules and keeps activity") {
    auto db = Database::open(":memory:");
    migrate(db, 5);
    ActivityRepository(db).addSeconds({{"2026-11-23", 600, Cat::Focus, "code", 60}});
    CHECK(db.userVersion() == 5);
    migrate(db);
    CHECK(db.userVersion() == 6);
    CHECK(ActivityRepository(db).forDay("2026-11-23").size() == 1);
    CHECK_NOTHROW(AppRuleRepository(db).set("code", Cat::Focus, 1));
}
