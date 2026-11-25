#include "storage/AppRuleRepository.h"

namespace focus {

std::map<std::string, ActivityCategory> AppRuleRepository::all() {
    std::map<std::string, ActivityCategory> out;
    auto st = db_.prepare("SELECT app, category FROM app_rules;");
    while (st.step()) {
        if (const auto c = parseCategory(st.columnText(1))) out[st.columnText(0)] = *c;
    }
    return out;
}

void AppRuleRepository::set(const std::string& app, ActivityCategory category, std::int64_t createdAt) {
    // The table only allows the four categories a user can choose; "away" is decided by the keyboard, not by an app.
    auto st = db_.prepare("INSERT OR REPLACE INTO app_rules (app, category, created_at) VALUES (?1, ?2, ?3);");
    st.bind(1, app).bind(2, std::string(categoryName(category))).bind(3, createdAt);
    st.run();
}

void AppRuleRepository::remove(const std::string& app) {
    auto st = db_.prepare("DELETE FROM app_rules WHERE app = ?1;");
    st.bind(1, app);
    st.run();
}

}  // namespace focus
