#include "storage/ActivityRepository.h"

#include <algorithm>

namespace focus {

void ActivityRepository::addSeconds(const std::vector<ActivityBucket>& buckets) {
    if (buckets.empty()) return;
    Transaction tx(db_);
    auto st = db_.prepare(
        "INSERT INTO activity_minutes (day, minute_of_day, category, app, seconds, active_s, switches, scroll_n, key_n, click_n)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)"
        " ON CONFLICT (day, minute_of_day, category, app) DO UPDATE SET"
        "   seconds   = seconds   + excluded.seconds,"
        "   active_s  = active_s  + excluded.active_s,"
        "   switches  = switches  + excluded.switches,"
        "   scroll_n  = scroll_n  + excluded.scroll_n,"
        "   key_n     = key_n     + excluded.key_n,"
        "   click_n   = click_n   + excluded.click_n;");
    for (const auto& b : buckets) {
        if (b.seconds <= 0) continue;
        // Input can never cover more time than the bucket has: a damaged value must not corrupt the totals.
        const int active = std::clamp(b.activeSeconds, 0, b.seconds);
        st.bind(1, b.day)
            .bind(2, b.minuteOfDay)
            .bind(3, std::string(categoryName(b.category)))
            .bind(4, b.app)
            .bind(5, b.seconds)
            .bind(6, active)
            .bind(7, std::max(b.switches, 0))
            .bind(8, std::max(b.scrollEvents, 0))
            .bind(9, std::max(b.keyEvents, 0))
            .bind(10, std::max(b.clickEvents, 0));
        st.run();
        st.reset();  // reuse the prepared statement for the next row
    }
    tx.commit();
}

std::vector<ActivityBucket> ActivityRepository::forDay(const std::string& day) {
    auto st = db_.prepare(
        "SELECT day, minute_of_day, category, app, seconds, active_s, switches, scroll_n, key_n, click_n"
        " FROM activity_minutes WHERE day = ?1 ORDER BY minute_of_day, category, app;");
    st.bind(1, day);
    std::vector<ActivityBucket> out;
    while (st.step()) {
        ActivityBucket b;
        b.day           = st.columnText(0);
        b.minuteOfDay   = static_cast<int>(st.columnInt(1));
        b.category      = parseCategory(st.columnText(2)).value_or(ActivityCategory::Neutral);
        b.app           = st.columnText(3);
        b.seconds       = static_cast<int>(st.columnInt(4));
        b.activeSeconds = static_cast<int>(st.columnInt(5));
        b.switches      = static_cast<int>(st.columnInt(6));
        b.scrollEvents  = static_cast<int>(st.columnInt(7));
        b.keyEvents     = static_cast<int>(st.columnInt(8));
        b.clickEvents   = static_cast<int>(st.columnInt(9));
        out.push_back(std::move(b));
    }
    return out;
}

void ActivityRepository::recategorize(const std::string& day, const std::string& app, std::optional<ActivityCategory> onlyFrom,
                                      ActivityCategory to) {
    if (to == ActivityCategory::Away) return;
    std::vector<ActivityBucket> toMove;
    for (auto& b : forDay(day)) {
        if (b.app != app || b.category == to || b.category == ActivityCategory::Away) continue;
        if (onlyFrom && b.category != *onlyFrom) continue;
        toMove.push_back(std::move(b));
    }
    if (toMove.empty()) return;

    Transaction tx(db_);
    auto del = db_.prepare("DELETE FROM activity_minutes WHERE day = ?1 AND minute_of_day = ?2 AND category = ?3 AND app = ?4;");
    for (const auto& b : toMove) {
        del.bind(1, b.day).bind(2, b.minuteOfDay).bind(3, std::string(categoryName(b.category))).bind(4, b.app);
        del.run();
        del.reset();
    }
    tx.commit();
    auto relabelled = toMove;
    for (auto& b : relabelled) b.category = to;
    addSeconds(relabelled);  // upsert: merges with any time already under the new category
}

void ActivityRepository::clearDay(const std::string& day) {
    auto st = db_.prepare("DELETE FROM activity_minutes WHERE day = ?1;");
    st.bind(1, day);
    st.run();
}

void ActivityRepository::clearAll() { db_.exec("DELETE FROM activity_minutes;"); }

}  // namespace focus
