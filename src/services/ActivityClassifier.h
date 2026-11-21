#pragma once
#include <string>
#include <vector>

#include "core/Activity.h"

namespace focus {

// "If the app is <process> and/or the window title contains <text>, it counts as <category>."
// Leave a field empty to ignore it. A rule with both fields empty never matches.
struct ActivityRule {
    ActivityCategory category = ActivityCategory::Neutral;
    std::string process;        // exact match, case-insensitive, ".exe" optional ("chrome" == "Chrome.exe")
    std::string titleContains;  // substring match, case-insensitive
};

// Turns a foreground window into an ActivityCategory using an ordered rule list.
// First matching rule wins, so order = priority. No match means Neutral.
//
// Privacy: only the resulting category is meant to be kept. Window titles can contain
// private text (emails, documents), so nothing here stores or logs them.
class ActivityClassifier {
public:
    explicit ActivityClassifier(std::vector<ActivityRule> rules = defaultRules());

    ActivityCategory classify(const ForegroundWindow& window) const;

    // AI tools first, then distractions, then focus tools.
    static std::vector<ActivityRule> defaultRules();

private:
    std::vector<ActivityRule> rules_;
};

}  // namespace focus
