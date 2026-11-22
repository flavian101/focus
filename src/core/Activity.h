#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace focus {

// What kind of thing was the user looking at? Used for time attribution and AI awareness.
enum class ActivityCategory {
    Focus,        // editor, terminal, docs you chose to work with
    Neutral,      // anything we have no opinion about
    Distraction,  // feeds, video, games
    AiAssistant,  // chat assistants (ChatGPT, Claude, Gemini, ...)
    Away,         // no keyboard/mouse input for a while (set from idle time, not from a window)
};

inline constexpr std::size_t kActivityCategoryCount = 5;

constexpr std::string_view categoryName(ActivityCategory c) {
    switch (c) {
        case ActivityCategory::Focus:       return "focus";
        case ActivityCategory::Neutral:     return "neutral";
        case ActivityCategory::Distraction: return "distraction";
        case ActivityCategory::AiAssistant: return "ai";
        case ActivityCategory::Away:        return "away";
    }
    return "neutral";
}

// What the OS says is in front of the user right now, as plain UTF-8 text.
// Platform code fills this in; services decide what it means. Core never touches Win32.
struct ForegroundWindow {
    std::string processName;  // e.g. "Code.exe"
    std::string title;        // e.g. "main.cpp - focus - Visual Studio Code"
};

}  // namespace focus
