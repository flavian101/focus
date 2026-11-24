#include <doctest/doctest.h>

#include "storage/CheckInRepository.h"
#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/SessionRepository.h"

using namespace focus;

namespace {
Database freshDb() {
    auto db = Database::open(":memory:");  // lives only in RAM; vanishes when the object dies
    migrate(db);
    return db;
}

SessionSummary sampleSession() {
    SessionSummary s;
    s.goal           = "ship the storage layer";
    s.approach       = "read the sqlite docs first";
    s.plannedSeconds = 1500;
    s.activeSeconds  = 1400;
    s.startedAt      = 1000;
    s.endedAt        = 2500;
    s.outcome        = SessionOutcome::Completed;
    s.aiBudget       = 2;
    s.aiVisits       = 1;
    s.aiManualLogs   = 1;
    s.secondsByCategory = {1000, 100, 50, 250, 75};  // focus, neutral, distraction, ai, away
    return s;
}
}  // namespace

TEST_CASE("migrate is idempotent and stamps the schema version") {
    auto db = Database::open(":memory:");
    CHECK(db.userVersion() == 0);
    migrate(db);
    CHECK(db.userVersion() == 5);
    CHECK_NOTHROW(migrate(db));  // second run changes nothing
    CHECK(db.userVersion() == 5);
}

TEST_CASE("a database from the future is refused") {
    auto db = Database::open(":memory:");
    db.setUserVersion(99);
    CHECK_THROWS_AS(migrate(db), StorageError);
}

TEST_CASE("sessions round-trip through storage") {
    auto db = freshDb();
    SessionRepository repo(db);
    const auto id = repo.insert(sampleSession());
    CHECK(id > 0);

    auto rows = repo.startedSince(0);
    REQUIRE(rows.size() == 1);
    const auto& r = rows[0];
    CHECK(r.id == id);
    CHECK(r.goal == "ship the storage layer");
    CHECK(r.approach == "read the sqlite docs first");
    CHECK(r.outcome == SessionOutcome::Completed);
    REQUIRE(r.aiBudget.has_value());
    CHECK(*r.aiBudget == 2);
    CHECK(r.aiTotal() == 2);
    CHECK(r.secondsIn(ActivityCategory::AiAssistant) == 250);
    CHECK(r.secondsIn(ActivityCategory::Away) == 75);
    CHECK(repo.startedSince(5000).empty());  // filter works
}

TEST_CASE("a NULL AI budget (no limit) survives the round trip") {
    auto db = freshDb();
    SessionRepository repo(db);
    auto s = sampleSession();
    s.aiBudget.reset();
    repo.insert(s);
    CHECK_FALSE(repo.startedSince(0)[0].aiBudget.has_value());
}

TEST_CASE("text with quotes is stored as data, never as SQL") {
    auto db = freshDb();
    SessionRepository repo(db);
    auto s = sampleSession();
    s.goal = "Robert'); DROP TABLE sessions;--";
    repo.insert(s);
    CHECK(repo.startedSince(0)[0].goal == s.goal);
}

TEST_CASE("check-ins round-trip and may exist without a session") {
    auto db = freshDb();
    SessionRepository sessions(db);
    CheckInRepository checkIns(db);
    const auto sid = sessions.insert(sampleSession());

    CheckIn withSession;
    withSession.sessionId = sid;
    withSession.kind      = CheckInKind::BeforeSession;
    withSession.mood      = 2;
    withSession.energy    = 4;
    withSession.tag       = "anxious";
    withSession.createdAt = 10;
    checkIns.insert(withSession);

    CheckIn standalone;
    standalone.mood      = 5;
    standalone.createdAt = 20;
    checkIns.insert(standalone);

    auto rows = checkIns.createdSince(0);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].sessionId == sid);
    CHECK(rows[0].kind == CheckInKind::BeforeSession);
    CHECK(rows[0].tag == "anxious");
    CHECK_FALSE(rows[1].sessionId.has_value());
}

TEST_CASE("the database rejects bad check-ins") {
    auto db = freshDb();
    CheckInRepository checkIns(db);

    CheckIn badMood;
    badMood.mood = 9;
    CHECK_THROWS_AS(checkIns.insert(badMood), StorageError);

    CheckIn orphan;
    orphan.sessionId = 12345;  // no such session: foreign key must refuse it
    CHECK_THROWS_AS(checkIns.insert(orphan), StorageError);
}

TEST_CASE("a Transaction that is not committed rolls back") {
    auto db = freshDb();
    SessionRepository repo(db);
    {
        Transaction tx(db);
        repo.insert(sampleSession());
        // leaves scope without commit()
    }
    CHECK(repo.startedSince(0).empty());
    {
        Transaction tx(db);
        repo.insert(sampleSession());
        tx.commit();
    }
    CHECK(repo.startedSince(0).size() == 1);
}

TEST_CASE("upgrading a v1 database keeps its data and gives new columns a default") {
    auto db = Database::open(":memory:");
    migrate(db, 1);  // an "old" database, as an earlier version of the app would have left it
    db.exec("INSERT INTO sessions (goal, planned_s, active_s, started_at, ended_at, outcome)"
            " VALUES ('written by v1', 60, 60, 1, 2, 'completed');");
    CHECK(db.userVersion() == 1);

    migrate(db);  // the upgrade a user gets when they install the new version
    CHECK(db.userVersion() == 5);

    auto rows = SessionRepository(db).startedSince(0);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].goal == "written by v1");
    CHECK(rows[0].secondsIn(ActivityCategory::Away) == 0);  // DEFAULT 0
}

TEST_CASE("recent() returns the newest sessions first and respects the limit") {
    auto db = freshDb();
    SessionRepository repo(db);
    for (int i = 1; i <= 5; ++i) {
        auto s      = sampleSession();
        s.goal      = "session " + std::to_string(i);
        s.startedAt = 1000 * i;
        repo.insert(s);
    }
    auto rows = repo.recent(3);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].goal == "session 5");
    CHECK(rows[2].goal == "session 3");
}
