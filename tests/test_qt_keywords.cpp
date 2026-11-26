#include <doctest/doctest.h>

// The standard headers first, as they would be in a Qt file. Only then are Qt's macros switched on.
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Qt turns a few ordinary words into macros in every file that includes a Qt header (see QT_NO_KEYWORDS).
// The UI includes our core and controller headers, so a parameter called `signals` compiles everywhere except
// in the UI build. Defining the same macros here makes that mistake fail in the plain test build too.
#define signals public
#define slots
#define emit

#include "controller/SessionController.h"
#include "core/ActivitySource.h"
#include "core/ActivityTracker.h"
#include "core/DaySummary.h"
#include "core/Tracking.h"

TEST_CASE("headers: Qt's keyword macros (signals, slots, emit) are not used as names in core or controller headers") {
    CHECK(true);  // the check is that this file compiles
}
