#pragma once
#include <optional>
#include <string>

#include "storage/Database.h"

namespace focus {

// Small user preferences, stored as text. Callers convert to the type they need.
class SettingsRepository {
public:
    explicit SettingsRepository(Database& db) : db_(db) {}

    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, const std::string& value);

    int getInt(const std::string& key, int fallback);  // fallback if missing or not a number
    void setInt(const std::string& key, int value) { set(key, std::to_string(value)); }

private:
    Database& db_;
};

}  // namespace focus
