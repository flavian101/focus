#pragma once
#include <string>
#include <vector>

#include "core/ActivityTracker.h"
#include "storage/Database.h"

namespace focus {

class ActivityRepository {
public:
    explicit ActivityRepository(Database& db) : db_(db) {}

    // Adds the buckets' seconds to what is already stored for the same minute, category and app.
    // One transaction, so a batch is saved completely or not at all.
    void addSeconds(const std::vector<ActivityBucket>& buckets);

    std::vector<ActivityBucket> forDay(const std::string& day);  // oldest minute first
    void clearDay(const std::string& day);
    void clearAll();

private:
    Database& db_;
};

}  // namespace focus
