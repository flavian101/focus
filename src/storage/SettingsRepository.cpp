#include "storage/SettingsRepository.h"

namespace focus {

std::optional<std::string> SettingsRepository::get(const std::string& key) {
    auto st = db_.prepare("SELECT value FROM settings WHERE key = ?1;");
    st.bind(1, key);
    if (!st.step()) return std::nullopt;
    return st.columnText(0);
}

void SettingsRepository::set(const std::string& key, const std::string& value) {
    auto st = db_.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?1, ?2);");
    st.bind(1, key).bind(2, value);
    st.run();
}

int SettingsRepository::getInt(const std::string& key, int fallback) {
    const auto text = get(key);
    if (!text) return fallback;
    try {
        std::size_t used = 0;
        const int value  = std::stoi(*text, &used);
        return used == text->size() ? value : fallback;
    } catch (const std::exception&) {
        return fallback;
    }
}

}  // namespace focus
