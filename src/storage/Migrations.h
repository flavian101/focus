#pragma once
#include "storage/Database.h"

namespace focus {

// Brings the database up to the newest schema. Safe to call on every start:
// it reads the file's version stamp and applies only the steps that are missing.
// Throws StorageError if the file was written by a NEWER version of the app.
void migrate(Database& db);

}  // namespace focus
