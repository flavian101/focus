#pragma once
#include "core/SessionManager.h"
#include "core/Statistics.h"

namespace focus {

void printSessionSummary(const SessionSummary& s);
void printReport(const StatsReport& r, int days);

}  // namespace focus
