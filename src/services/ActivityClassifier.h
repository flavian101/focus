#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/Activity.h"

namespace focus {

// "Code.exe" -> "code": lower case, without ".exe". The same name is used for rules and for activity history.
std::string normalizedProcessName(std::string_view processName);

// Is this a web browser? Browsers host many different things (docs, an AI chat, a video feed), so
// one label for the whole app would be wrong. See ActivityClassifier::setOverrides().
bool isBrowserProcess(std::string_view processName);

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

    // The user's own corrections: "this app is <category>". They win over the built-in rules, with
    // one exception: for a BROWSER the override only replaces "no opinion" (Neutral), so an AI chat
    // or a video feed in a tab is still recognised. Keys are normalised app names ("code", not "Code.exe").
    void setOverrides(std::map<std::string, ActivityCategory> overrides) { overrides_ = std::move(overrides); }

    // AI tools first, then distractions, then focus tools.
    static std::vector<ActivityRule> defaultRules();

private:
    std::vector<ActivityRule> rules_;
    std::map<std::string, ActivityCategory> overrides_;
};

}  // namespace focus
