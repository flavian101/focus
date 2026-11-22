#include "storage/CheckInRepository.h"

namespace focus {

std::int64_t CheckInRepository::insert(const CheckIn& c) {
    if (!c.isValid()) throw StorageError("check-in mood/energy must be between 1 and 5");

    auto st = db_.prepare(
        "INSERT INTO checkins (session_id, kind, mood, energy, tag, note, created_at)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);");
    if (c.sessionId) st.bind(1, *c.sessionId); else st.bindNull(1);
    st.bind(2, std::string(kindName(c.kind)))
        .bind(3, c.mood)
        .bind(4, c.energy)
        .bind(5, c.tag)
        .bind(6, c.note)
        .bind(7, c.createdAt);
    st.run();
    return db_.lastInsertRowId();
}

std::vector<CheckIn> CheckInRepository::createdSince(std::int64_t unixSeconds) {
    auto st = db_.prepare(
        "SELECT id, session_id, kind, mood, energy, tag, note, created_at"
        " FROM checkins WHERE created_at >= ?1 ORDER BY created_at, id;");
    st.bind(1, unixSeconds);

    std::vector<CheckIn> out;
    while (st.step()) {
        CheckIn c;
        c.id = st.columnInt(0);
        if (!st.columnIsNull(1)) c.sessionId = st.columnInt(1);
        c.kind      = parseCheckInKind(st.columnText(2)).value_or(CheckInKind::Manual);
        c.mood      = static_cast<int>(st.columnInt(3));
        c.energy    = static_cast<int>(st.columnInt(4));
        c.tag       = st.columnText(5);
        c.note      = st.columnText(6);
        c.createdAt = st.columnInt(7);
        out.push_back(std::move(c));
    }
    return out;
}

}  // namespace focus
