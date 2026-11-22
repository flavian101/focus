#pragma once
#include <string>

#include "core/CheckIn.h"

namespace focus {

// Asks a question and returns the trimmed answer. Returns "" on empty input or end-of-input.
std::string promptLine(const std::string& label);

// Asks for a whole number in [lo, hi]. Enter = `fallback`. Re-asks on garbage.
int promptRange(const std::string& label, int lo, int hi, int fallback);

// Mood (1-5), energy (1-5) and an optional one-word feeling.
CheckIn promptCheckIn(CheckInKind kind);

}  // namespace focus
