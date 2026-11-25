#include <doctest/doctest.h>

#include "services/ActivityClassifier.h"

using namespace focus;

namespace {
ActivityCategory classify(const std::string& process, const std::string& title) {
    static const ActivityClassifier classifier;
    return classifier.classify({process, title});
}
}  // namespace

TEST_CASE("AI tools are recognised in a browser tab by the tab title") {
    CHECK(classify("chrome.exe", "Claude - Google Chrome") == ActivityCategory::AiAssistant);
    CHECK(classify("msedge.exe", "ChatGPT - Microsoft Edge") == ActivityCategory::AiAssistant);
    CHECK(classify("firefox.exe", "Gemini - Mozilla Firefox") == ActivityCategory::AiAssistant);
}

TEST_CASE("an AI word in a non-browser window is not an AI visit") {
    // Editing a file called claude-notes.md must not count as asking an AI.
    CHECK(classify("Code.exe", "claude-notes.md - focus - Visual Studio Code") == ActivityCategory::Focus);
}

TEST_CASE("native AI apps are recognised by process") {
    CHECK(classify("Claude.exe", "Claude") == ActivityCategory::AiAssistant);
}

TEST_CASE("distractions and focus tools") {
    CHECK(classify("chrome.exe", "Lofi Girl - YouTube - Google Chrome") == ActivityCategory::Distraction);
    CHECK(classify("devenv.exe", "focus - Microsoft Visual Studio") == ActivityCategory::Focus);
    CHECK(classify("WindowsTerminal.exe", "pwsh") == ActivityCategory::Focus);
}

TEST_CASE("unknown windows are neutral") {
    CHECK(classify("chrome.exe", "cppreference.com - Google Chrome") == ActivityCategory::Neutral);
    CHECK(classify("", "") == ActivityCategory::Neutral);
}

TEST_CASE("matching is case-insensitive and .exe is optional") {
    CHECK(classify("CODE", "x") == ActivityCategory::Focus);
    CHECK(classify("Chrome", "CLAUDE") == ActivityCategory::AiAssistant);
}

TEST_CASE("custom rules: first match wins and empty rules never match") {
    ActivityClassifier c({
        {ActivityCategory::Focus, "chrome", "docs.rs"},       // work in the browser
        {ActivityCategory::Distraction, "chrome", ""},         // everything else in chrome
        {ActivityCategory::AiAssistant, "", ""},               // ignored: matches nothing
    });
    CHECK(c.classify({"chrome.exe", "serde - docs.rs"}) == ActivityCategory::Focus);
    CHECK(c.classify({"chrome.exe", "memes"}) == ActivityCategory::Distraction);
    CHECK(c.classify({"notepad.exe", "memes"}) == ActivityCategory::Neutral);
}

TEST_CASE("a user override relabels an app that the built-in rules had no opinion about") {
    ActivityClassifier c;
    CHECK(c.classify({"figma.exe", "Design"}) == ActivityCategory::Neutral);
    c.setOverrides({{"figma", ActivityCategory::Focus}});
    CHECK(c.classify({"Figma.exe", "Design"}) == ActivityCategory::Focus);
}

TEST_CASE("a user override beats a built-in rule for a normal app") {
    ActivityClassifier c;
    CHECK(c.classify({"code.exe", "x"}) == ActivityCategory::Focus);
    c.setOverrides({{"code", ActivityCategory::Distraction}});  // the user says so
    CHECK(c.classify({"code.exe", "x"}) == ActivityCategory::Distraction);
}

TEST_CASE("a browser override is only the default: AI and distraction tabs are still recognised") {
    ActivityClassifier c;
    c.setOverrides({{"chrome", ActivityCategory::Focus}});                 // "my browser is for work"
    CHECK(c.classify({"chrome.exe", "Docs - Google Chrome"}) == ActivityCategory::Focus);
    CHECK(c.classify({"chrome.exe", "Claude - Google Chrome"}) == ActivityCategory::AiAssistant);
    CHECK(c.classify({"chrome.exe", "Funny cats - YouTube - Google Chrome"}) == ActivityCategory::Distraction);
}

TEST_CASE("browsers are recognised by process name") {
    CHECK(isBrowserProcess("Chrome.exe"));
    CHECK(isBrowserProcess("msedge"));
    CHECK_FALSE(isBrowserProcess("code.exe"));
}
