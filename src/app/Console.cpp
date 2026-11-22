#include "app/Console.h"

#include <algorithm>
#include <iostream>

#include "app/TimeUtil.h"

namespace focus {

namespace {
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}
}  // namespace

std::string promptLine(const std::string& label) {
    std::cout << label << ": " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return {};  // end of input (Ctrl+Z / piped stdin)
    return trim(line);
}

int promptRange(const std::string& label, int lo, int hi, int fallback) {
    while (true) {
        std::cout << label << " [" << lo << "-" << hi << ", Enter=" << fallback << "]: " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return fallback;  // no input left: don't loop forever
        line = trim(line);
        if (line.empty()) return fallback;
        try {
            std::size_t used = 0;
            const int value  = std::stoi(line, &used);
            if (used == line.size() && value >= lo && value <= hi) return value;
        } catch (const std::exception&) {
            // fall through: not a number
        }
        std::cout << "  please enter a number from " << lo << " to " << hi << "\n";
    }
}

CheckIn promptCheckIn(CheckInKind kind) {
    std::cout << (kind == CheckInKind::BeforeSession ? "\nQuick check-in before you start\n"
                                                      : "\nQuick check-in: how was that?\n");
    CheckIn c;
    c.kind   = kind;
    c.mood   = promptRange("  Mood   (1 awful .. 5 great)", 1, 5, 3);
    c.energy = promptRange("  Energy (1 drained .. 5 energised)", 1, 5, 3);
    c.tag    = promptLine("  One word for the feeling (anxious, stuck, bored, calm... Enter to skip)");
    if (c.tag.size() > 32) c.tag.resize(32);
    c.createdAt = unixNow();
    return c;
}

}  // namespace focus
