#include "storage/Migrations.h"

#include <algorithm>
#include <array>

namespace focus {

namespace {

// Rule: never edit a step that has shipped. To change the schema, append a new step.
// Step N takes the database from version N to version N+1.
constexpr std::array<const char*, 2> kSteps = {
    // v0 -> v1
    R"sql(
    CREATE TABLE sessions (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        goal          TEXT    NOT NULL,
        approach      TEXT    NOT NULL DEFAULT '',
        planned_s     INTEGER NOT NULL,
        active_s      INTEGER NOT NULL,
        started_at    INTEGER NOT NULL,
        ended_at      INTEGER NOT NULL,
        outcome       TEXT    NOT NULL CHECK (outcome IN ('completed', 'abandoned')),
        ai_budget     INTEGER,                      -- NULL = no limit
        ai_visits     INTEGER NOT NULL DEFAULT 0,
        ai_manual     INTEGER NOT NULL DEFAULT 0,
        focus_s       INTEGER NOT NULL DEFAULT 0,
        neutral_s     INTEGER NOT NULL DEFAULT 0,
        distraction_s INTEGER NOT NULL DEFAULT 0,
        ai_s          INTEGER NOT NULL DEFAULT 0
    );
    CREATE INDEX idx_sessions_started ON sessions (started_at);

    CREATE TABLE checkins (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id INTEGER REFERENCES sessions (id) ON DELETE CASCADE,
        kind       TEXT    NOT NULL CHECK (kind IN ('before', 'after', 'manual')),
        mood       INTEGER NOT NULL CHECK (mood   BETWEEN 1 AND 5),
        energy     INTEGER NOT NULL CHECK (energy BETWEEN 1 AND 5),
        tag        TEXT    NOT NULL DEFAULT '',
        note       TEXT    NOT NULL DEFAULT '',
        created_at INTEGER NOT NULL
    );
    CREATE INDEX idx_checkins_session ON checkins (session_id);
    CREATE INDEX idx_checkins_created ON checkins (created_at);
    )sql",
    // v1 -> v2: time with no keyboard/mouse input. DEFAULT 0 gives every existing row a valid value.
    R"sql(
    ALTER TABLE sessions ADD COLUMN away_s INTEGER NOT NULL DEFAULT 0;
    )sql",
};

}  // namespace

void migrate(Database& db, std::optional<int> upTo) {
    const int current = db.userVersion();
    const int latest  = static_cast<int>(kSteps.size());
    if (current > latest) {
        throw StorageError("database schema v" + std::to_string(current) +
                           " is newer than this app understands (v" + std::to_string(latest) + ")");
    }
    const int target = upTo ? std::min(*upTo, latest) : latest;
    for (int v = current; v < target; ++v) {
        Transaction tx(db);  // a step either fully applies, or not at all
        db.exec(kSteps[static_cast<std::size_t>(v)]);
        db.setUserVersion(v + 1);
        tx.commit();
    }
}

}  // namespace focus
