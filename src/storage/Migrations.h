#pragma once
#include <optional>

#include "storage/Database.h"

namespace focus {

// Brings the database up to the newest schema (or to `upTo`, which exists so tests can
// build an "old" database and then upgrade it). Safe to call on every start:
// it reads the file's version stamp and applies only the steps that are missing.
// Throws StorageError if the file was written by a NEWER version of the app.
void migrate(Database& db, std::optional<int> upTo = std::nullopt);

}  // namespace focus
