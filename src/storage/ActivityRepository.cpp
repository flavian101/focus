#include "storage/ActivityRepository.h"

namespace focus {

void ActivityRepository::addSeconds(const std::vector<ActivityBucket>& buckets) {
    if (buckets.empty()) return;
    Transaction tx(db_);
    auto st = db_.prepare(
        "INSERT INTO activity_minutes (day, minute_of_day, category, app, seconds) VALUES (?1, ?2, ?3, ?4, ?5)"
        " ON CONFLICT (day, minute_of_day, category, app) DO UPDATE SET seconds = seconds + excluded.seconds;");
    for (const auto& b : buckets) {
        if (b.seconds <= 0) continue;
        st.bind(1, b.day)
            .bind(2, b.minuteOfDay)
            .bind(3, std::string(categoryName(b.category)))
            .bind(4, b.app)
            .bind(5, b.seconds);
        st.run();
        st.reset();  // reuse the prepared statement for the next row
    }
    tx.commit();
}

std::vector<ActivityBucket> ActivityRepository::forDay(const std::string& day) {
    auto st = db_.prepare(
        "SELECT day, minute_of_day, category, app, seconds FROM activity_minutes WHERE day = ?1"
        " ORDER BY minute_of_day, category, app;");
    st.bind(1, day);
    std::vector<ActivityBucket> out;
    while (st.step()) {
        ActivityBucket b;
        b.day         = st.columnText(0);
        b.minuteOfDay = static_cast<int>(st.columnInt(1));
        b.category    = parseCategory(st.columnText(2)).value_or(ActivityCategory::Neutral);
        b.app         = st.columnText(3);
        b.seconds     = static_cast<int>(st.columnInt(4));
        out.push_back(std::move(b));
    }
    return out;
}

void ActivityRepository::clearDay(const std::string& day) {
    auto st = db_.prepare("DELETE FROM activity_minutes WHERE day = ?1;");
    st.bind(1, day);
    st.run();
}

void ActivityRepository::clearAll() { db_.exec("DELETE FROM activity_minutes;"); }

}  // namespace focus
