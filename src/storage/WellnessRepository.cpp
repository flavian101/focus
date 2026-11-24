#include "storage/WellnessRepository.h"

namespace focus {

void WellnessRepository::addGlass(const std::string& day, int minuteOfDay, std::int64_t loggedAt) {
    auto st = db_.prepare("INSERT INTO water_log (day, glasses, minute_of_day, logged_at) VALUES (?1, 1, ?2, ?3);");
    st.bind(1, day).bind(2, minuteOfDay).bind(3, loggedAt);
    st.run();
}

bool WellnessRepository::undoLastGlass(const std::string& day) {
    auto find = db_.prepare("SELECT id FROM water_log WHERE day = ?1 ORDER BY id DESC LIMIT 1;");
    find.bind(1, day);
    if (!find.step()) return false;
    const auto id = find.columnInt(0);

    auto del = db_.prepare("DELETE FROM water_log WHERE id = ?1;");
    del.bind(1, id);
    del.run();
    return true;
}

WaterFacts WellnessRepository::waterFor(const std::string& day) {
    WaterFacts facts;
    auto total = db_.prepare("SELECT COALESCE(SUM(glasses), 0) FROM water_log WHERE day = ?1;");
    total.bind(1, day);
    if (total.step()) facts.glasses = static_cast<int>(total.columnInt(0));

    auto last = db_.prepare("SELECT minute_of_day FROM water_log WHERE day = ?1 ORDER BY id DESC LIMIT 1;");
    last.bind(1, day);
    if (last.step()) facts.lastGlassMinute = static_cast<int>(last.columnInt(0));
    return facts;
}

void WellnessRepository::setMeal(const std::string& day, const std::string& mealId, MealStatus status, std::int64_t loggedAt) {
    // INSERT OR REPLACE + the UNIQUE (day, meal_id) constraint: changing your answer overwrites it.
    auto st = db_.prepare("INSERT OR REPLACE INTO meal_log (day, meal_id, status, logged_at) VALUES (?1, ?2, ?3, ?4);");
    st.bind(1, day).bind(2, mealId).bind(3, std::string(status == MealStatus::Ate ? "ate" : "skipped")).bind(4, loggedAt);
    st.run();
}

void WellnessRepository::clearMeal(const std::string& day, const std::string& mealId) {
    auto st = db_.prepare("DELETE FROM meal_log WHERE day = ?1 AND meal_id = ?2;");
    st.bind(1, day).bind(2, mealId);
    st.run();
}

std::map<std::string, MealStatus> WellnessRepository::mealsFor(const std::string& day) {
    std::map<std::string, MealStatus> out;
    auto st = db_.prepare("SELECT meal_id, status FROM meal_log WHERE day = ?1;");
    st.bind(1, day);
    while (st.step()) out[st.columnText(0)] = st.columnText(1) == "ate" ? MealStatus::Ate : MealStatus::Skipped;
    return out;
}

}  // namespace focus
