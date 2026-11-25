#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "core/Activity.h"
#include "storage/Database.h"

namespace focus {

// "This app is <category>", chosen by the user. Keys are normalised app names ("code").
class AppRuleRepository {
public:
    explicit AppRuleRepository(Database& db) : db_(db) {}

    std::map<std::string, ActivityCategory> all();
    void set(const std::string& app, ActivityCategory category, std::int64_t createdAt);  // replaces an existing rule
    void remove(const std::string& app);

private:
    Database& db_;
};

}  // namespace focus
