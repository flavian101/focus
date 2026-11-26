#pragma once
#include <optional>
#include <string>
#include <string_view>

#include "core/Wellness.h"  // LocalTime

namespace focus {

// How much detail all-day tracking keeps. Less detail = more privacy, so the default outside work is Private.
enum class TrackingMode {
    Off,       // nothing is recorded
    Private,   // category and time only ("40 min of distraction"), never which app
    Work,      // category AND app name ("code", "chrome")
    Detailed,  // everything in Work, plus how many scrolls, key presses and clicks happened (counts only)
};

constexpr std::string_view modeName(TrackingMode m) {
    switch (m) {
        case TrackingMode::Off:      return "off";
        case TrackingMode::Private:  return "private";
        case TrackingMode::Work:     return "work";
        case TrackingMode::Detailed: return "detailed";
    }
    return "private";
}

constexpr std::optional<TrackingMode> parseMode(std::string_view s) {
    if (s == "off")      return TrackingMode::Off;
    if (s == "private")  return TrackingMode::Private;
    if (s == "work")     return TrackingMode::Work;
    if (s == "detailed") return TrackingMode::Detailed;
    return std::nullopt;
}

// What each mode is allowed to keep. This is the one place that decides, so the rest of the code
// asks a question ("may I keep the app name?") instead of comparing modes by hand.
constexpr bool keepsCategory(TrackingMode m) { return m != TrackingMode::Off; }
constexpr bool keepsAppName(TrackingMode m) { return m == TrackingMode::Work || m == TrackingMode::Detailed; }
// Engagement basics: seconds with any keyboard/mouse input, and how often the foreground window changed.
// Neither says which app or what was typed, so they are safe wherever the category is kept.
constexpr bool keepsEngagement(TrackingMode m) { return keepsCategory(m); }
// Counts of scrolls, key presses and clicks. Only the highest level listens for these at all.
constexpr bool keepsInputCounts(TrackingMode m) { return m == TrackingMode::Detailed; }

// 0 = Monday ... 6 = Sunday for a "YYYY-MM-DD" date, or -1 if the text is not a real date.
// Pure arithmetic, so core still needs no calendar or time zone from the OS.
int weekdayOfIso(const std::string& day);

// When "work hours" are. Days are a bit mask: bit 0 = Monday ... bit 6 = Sunday.
struct WorkSchedule {
    unsigned days   = 0b0011111;  // Monday to Friday
    int startMinute = 9 * 60;
    int endMinute   = 17 * 60;    // the end is exclusive

    bool contains(const LocalTime& t) const;
};

struct TrackingPolicy {
    TrackingMode duringWork = TrackingMode::Work;
    TrackingMode otherwise  = TrackingMode::Private;
    WorkSchedule schedule;

    TrackingMode modeAt(const LocalTime& t) const { return schedule.contains(t) ? duringWork : otherwise; }
};

// Is this policy sane? (start before end, times inside the day, at least one work day if it matters.)
bool valid(const TrackingPolicy& policy);

// Free text about the user, kept on this computer. Gives later features context: the same app
// can be work or play depending on what the computer is for.
struct UserProfile {
    std::string role;     // e.g. "software engineering intern"
    std::string purpose;  // e.g. "Mostly coding. Browser is for docs and research."
};

// Trims whitespace and enforces length limits.
UserProfile cleaned(UserProfile profile);

}  // namespace focus
