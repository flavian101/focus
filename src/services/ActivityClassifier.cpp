#include "services/ActivityClassifier.h"

#include <array>
#include <string_view>

namespace focus {

namespace {

std::string lowerAscii(std::string_view in) {
    std::string out(in);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// "Code.exe" -> "code"
std::string normalizeProcess(std::string_view name) {
    std::string s = lowerAscii(name);
    constexpr std::string_view kExe = ".exe";
    if (s.size() >= kExe.size() && s.compare(s.size() - kExe.size(), kExe.size(), kExe) == 0) {
        s.resize(s.size() - kExe.size());
    }
    return s;
}

// Browsers only show the ACTIVE tab's title, which is exactly what we want to match on.
constexpr std::array<std::string_view, 6> kBrowsers = {"chrome", "msedge", "firefox", "brave", "opera", "vivaldi"};

constexpr std::array<std::string_view, 6> kAiSites = {"chatgpt", "claude", "gemini", "perplexity", "deepseek", "copilot"};
constexpr std::array<std::string_view, 2> kAiApps = {"claude", "chatgpt"};

constexpr std::array<std::string_view, 8> kDistractionSites = {
    "youtube", "netflix", "reddit", "tiktok", "instagram", "facebook", "twitch", "twitter"};

constexpr std::array<std::string_view, 12> kFocusApps = {
    "code", "devenv", "clion64", "rider64", "idea64", "pycharm64",
    "windowsterminal", "wt", "cmd", "powershell", "pwsh", "notepad++"};

}  // namespace

ActivityClassifier::ActivityClassifier(std::vector<ActivityRule> rules) : rules_(std::move(rules)) {
    // Normalize once up front so classify() can compare cheaply.
    for (auto& r : rules_) {
        r.process       = normalizeProcess(r.process);
        r.titleContains = lowerAscii(r.titleContains);
    }
}

ActivityCategory ActivityClassifier::classify(const ForegroundWindow& window) const {
    const std::string process = normalizeProcess(window.processName);
    const std::string title   = lowerAscii(window.title);

    for (const auto& r : rules_) {
        if (r.process.empty() && r.titleContains.empty()) continue;
        if (!r.process.empty() && r.process != process) continue;
        if (!r.titleContains.empty() && title.find(r.titleContains) == std::string::npos) continue;
        return r.category;
    }
    return ActivityCategory::Neutral;
}

std::vector<ActivityRule> ActivityClassifier::defaultRules() {
    std::vector<ActivityRule> rules;

    // 1) AI assistants: native apps, then browser tabs.
    for (auto app : kAiApps) {
        rules.push_back({ActivityCategory::AiAssistant, std::string(app), ""});
    }
    for (auto browser : kBrowsers) {
        for (auto site : kAiSites) {
            rules.push_back({ActivityCategory::AiAssistant, std::string(browser), std::string(site)});
        }
    }
    // 2) Distractions in browsers.
    for (auto browser : kBrowsers) {
        for (auto site : kDistractionSites) {
            rules.push_back({ActivityCategory::Distraction, std::string(browser), std::string(site)});
        }
    }
    // 3) Tools you chose for the job.
    for (auto app : kFocusApps) {
        rules.push_back({ActivityCategory::Focus, std::string(app), ""});
    }
    return rules;
}

}  // namespace focus
