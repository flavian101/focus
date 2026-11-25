#include "storage/Migrations.h"

#include <algorithm>
#include <array>

namespace focus {

namespace {

// Rule: never edit a step that has shipped. To change the schema, append a new step.
// Step N takes the database from version N to version N+1.
constexpr std::array<const char*, 6> kSteps = {
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
    // v2 -> v3: the day plan. A session may point at the task it worked on; if the task is
    // ever deleted the session stays and simply loses the link (ON DELETE SET NULL).
    R"sql(
    CREATE TABLE tasks (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        title        TEXT    NOT NULL,
        priority     INTEGER NOT NULL CHECK (priority BETWEEN 1 AND 3),
        estimate_min INTEGER NOT NULL CHECK (estimate_min > 0),
        status       TEXT    NOT NULL DEFAULT 'todo' CHECK (status IN ('todo', 'done', 'dropped')),
        day          TEXT    NOT NULL,              -- local calendar day, 'YYYY-MM-DD'
        created_at   INTEGER NOT NULL,
        completed_at INTEGER                        -- NULL until done
    );
    CREATE INDEX idx_tasks_day ON tasks (day);

    ALTER TABLE sessions ADD COLUMN task_id INTEGER REFERENCES tasks (id) ON DELETE SET NULL;
    CREATE INDEX idx_sessions_task ON sessions (task_id);
    )sql",
    // v3 -> v4: water and meals, plus a small key/value table for user settings.
    R"sql(
    CREATE TABLE water_log (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        day           TEXT    NOT NULL,             -- local calendar day, 'YYYY-MM-DD'
        glasses       INTEGER NOT NULL CHECK (glasses > 0),
        minute_of_day INTEGER NOT NULL CHECK (minute_of_day BETWEEN 0 AND 1439),
        logged_at     INTEGER NOT NULL
    );
    CREATE INDEX idx_water_day ON water_log (day);

    CREATE TABLE meal_log (
        id        INTEGER PRIMARY KEY AUTOINCREMENT,
        day       TEXT    NOT NULL,
        meal_id   TEXT    NOT NULL,
        status    TEXT    NOT NULL CHECK (status IN ('ate', 'skipped')),
        logged_at INTEGER NOT NULL,
        UNIQUE (day, meal_id)                       -- one answer per meal per day; changing it replaces it
    );

    CREATE TABLE settings (
        key   TEXT PRIMARY KEY,
        value TEXT NOT NULL
    );
    )sql",
    // v4 -> v5: all-day activity, one row per minute, category and app. App NAMES only (never window
    // titles). The primary key makes "add seconds to this minute" a single upsert.
    R"sql(
    CREATE TABLE activity_minutes (
        day           TEXT    NOT NULL,             -- local calendar day, 'YYYY-MM-DD'
        minute_of_day INTEGER NOT NULL CHECK (minute_of_day BETWEEN 0 AND 1439),
        category      TEXT    NOT NULL CHECK (category IN ('focus', 'neutral', 'distraction', 'ai', 'away')),
        app           TEXT    NOT NULL,             -- e.g. 'code', 'chrome'; empty for 'away'
        seconds       INTEGER NOT NULL CHECK (seconds > 0),
        PRIMARY KEY (day, minute_of_day, category, app)
    ) WITHOUT ROWID;
    )sql",
    // v5 -> v6: the user's own corrections ("this app is focus work"). App names only.
    R"sql(
    CREATE TABLE app_rules (
        app        TEXT PRIMARY KEY,
        category   TEXT    NOT NULL CHECK (category IN ('focus', 'neutral', 'distraction', 'ai')),
        created_at INTEGER NOT NULL
    );
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
