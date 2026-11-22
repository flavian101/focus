#pragma once
#include <string>

namespace focus::win {

// %LOCALAPPDATA%\Focus, created if missing, as UTF-8.
// User data belongs here, not next to the .exe: Program Files is read-only for normal
// users, and deleting or reinstalling the app must not delete your history.
// Returns an empty string if the folder cannot be determined or created.
std::string appDataDirectory();

}  // namespace focus::win
