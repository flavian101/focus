#pragma once
#include <cstdint>
#include <vector>

#include "core/SessionManager.h"
#include "storage/Database.h"

namespace focus {

class SessionRepository {
public:
    explicit SessionRepository(Database& db) : db_(db) {}

    std::int64_t insert(const SessionSummary& session);  // returns the new row id
    std::vector<SessionSummary> startedSince(std::int64_t unixSeconds);  // oldest first
    std::vector<SessionSummary> recent(int limit);                       // newest first

private:
    Database& db_;
};

}  // namespace focus
