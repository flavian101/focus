#pragma once
#include <cstdint>
#include <vector>

#include "core/CheckIn.h"
#include "storage/Database.h"

namespace focus {

class CheckInRepository {
public:
    explicit CheckInRepository(Database& db) : db_(db) {}

    // Throws StorageError if the check-in is invalid or points at a session that does not exist.
    std::int64_t insert(const CheckIn& checkIn);
    std::vector<CheckIn> createdSince(std::int64_t unixSeconds);

private:
    Database& db_;
};

}  // namespace focus
