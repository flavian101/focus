#include "storage/SessionRepository.h"

namespace focus {

namespace {
const char* outcomeName(SessionOutcome o) {
    return o == SessionOutcome::Completed ? "completed" : "abandoned";
}
constexpr std::size_t idx(ActivityCategory c) { return static_cast<std::size_t>(c); }
}  // namespace

std::int64_t SessionRepository::insert(const SessionSummary& s) {
    auto st = db_.prepare(
        "INSERT INTO sessions (goal, approach, planned_s, active_s, started_at, ended_at, outcome,"
        " ai_budget, ai_visits, ai_manual, focus_s, neutral_s, distraction_s, ai_s, away_s, task_id)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16);");
    st.bind(1, s.goal)
        .bind(2, s.approach)
        .bind(3, s.plannedSeconds)
        .bind(4, s.activeSeconds)
        .bind(5, s.startedAt)
        .bind(6, s.endedAt)
        .bind(7, std::string(outcomeName(s.outcome)));
    if (s.aiBudget) st.bind(8, *s.aiBudget); else st.bindNull(8);
    st.bind(9, s.aiVisits)
        .bind(10, s.aiManualLogs)
        .bind(11, s.secondsByCategory[idx(ActivityCategory::Focus)])
        .bind(12, s.secondsByCategory[idx(ActivityCategory::Neutral)])
        .bind(13, s.secondsByCategory[idx(ActivityCategory::Distraction)])
        .bind(14, s.secondsByCategory[idx(ActivityCategory::AiAssistant)])
        .bind(15, s.secondsByCategory[idx(ActivityCategory::Away)]);
    if (s.taskId) st.bind(16, *s.taskId); else st.bindNull(16);
    st.run();
    return db_.lastInsertRowId();
}

namespace {

constexpr const char* kSelectColumns =
    "SELECT id, goal, approach, planned_s, active_s, started_at, ended_at, outcome,"
    " ai_budget, ai_visits, ai_manual, focus_s, neutral_s, distraction_s, ai_s, away_s, task_id FROM sessions";

SessionSummary readSession(Statement& st) {
    SessionSummary s;
    s.id             = st.columnInt(0);
    s.goal           = st.columnText(1);
    s.approach       = st.columnText(2);
    s.plannedSeconds = st.columnInt(3);
    s.activeSeconds  = st.columnInt(4);
    s.startedAt      = st.columnInt(5);
    s.endedAt        = st.columnInt(6);
    s.outcome        = st.columnText(7) == "completed" ? SessionOutcome::Completed : SessionOutcome::Abandoned;
    if (!st.columnIsNull(8)) s.aiBudget = static_cast<int>(st.columnInt(8));
    s.aiVisits     = static_cast<int>(st.columnInt(9));
    s.aiManualLogs = static_cast<int>(st.columnInt(10));
    s.secondsByCategory[idx(ActivityCategory::Focus)]       = st.columnInt(11);
    s.secondsByCategory[idx(ActivityCategory::Neutral)]     = st.columnInt(12);
    s.secondsByCategory[idx(ActivityCategory::Distraction)] = st.columnInt(13);
    s.secondsByCategory[idx(ActivityCategory::AiAssistant)] = st.columnInt(14);
    s.secondsByCategory[idx(ActivityCategory::Away)]        = st.columnInt(15);
    if (!st.columnIsNull(16)) s.taskId = st.columnInt(16);
    return s;
}

}  // namespace

std::vector<SessionSummary> SessionRepository::startedSince(std::int64_t unixSeconds) {
    auto st = db_.prepare(std::string(kSelectColumns) + " WHERE started_at >= ?1 ORDER BY started_at;");
    st.bind(1, unixSeconds);

    std::vector<SessionSummary> out;
    while (st.step()) out.push_back(readSession(st));
    return out;
}

std::vector<SessionSummary> SessionRepository::recent(int limit) {
    auto st = db_.prepare(std::string(kSelectColumns) + " ORDER BY started_at DESC, id DESC LIMIT ?1;");
    st.bind(1, limit);

    std::vector<SessionSummary> out;
    while (st.step()) out.push_back(readSession(st));
    return out;
}

}  // namespace focus
