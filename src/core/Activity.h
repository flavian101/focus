#pragma once
#include <cstddef>
#include <string_view>

namespace focus {

// What kind of thing was the user looking at? Used for time attribution and AI awareness.
enum class ActivityCategory {
    Focus,        // editor, terminal, docs you chose to work with
    Neutral,      // anything we have no opinion about
    Distraction,  // feeds, video, games
    AiAssistant,  // chat assistants (ChatGPT, Claude, Gemini, ...)
};

inline constexpr std::size_t kActivityCategoryCount = 4;

constexpr std::string_view toString(ActivityCategory c) {
    switch (c) {
        case ActivityCategory::Focus:       return "focus";
        case ActivityCategory::Neutral:     return "neutral";
        case ActivityCategory::Distraction: return "distraction";
        case ActivityCategory::AiAssistant: return "ai";
    }
    return "neutral";
}

}  // namespace focus
