#include <windows.h>

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "app/Application.h"
#include "app/Console.h"
#include "app/Report.h"
#include "app/TimeUtil.h"
#include "core/Statistics.h"
#include "platform/Paths.h"
#include "platform/Utf8.h"
#include "storage/CheckInRepository.h"
#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/SessionRepository.h"

namespace {

using Args = std::vector<std::string>;

void printUsage() {
    std::cout << "Usage:\n"
                 "  focus start [--goal TEXT] [--approach TEXT] [--minutes N] [--ai-budget N|none]\n"
                 "  focus stats [--days N]\n"
                 "  focus help\n";
}

// Finds "--name value" and returns value.
std::optional<std::string> option(const Args& args, const std::string& name) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) return args[i + 1];
    }
    return std::nullopt;
}

std::optional<int> intOption(const Args& args, const std::string& name) {
    const auto text = option(args, name);
    if (!text) return std::nullopt;
    try {
        std::size_t used = 0;
        const int value  = std::stoi(*text, &used);
        if (used == text->size()) return value;
    } catch (const std::exception&) {
    }
    std::cerr << "Ignoring invalid value for " << name << ": " << *text << "\n";
    return std::nullopt;
}

int commandStart(focus::Database& db, const Args& args) {
    const auto config = focus::Configuration::defaults();

    focus::SessionPlan plan;
    plan.goal = option(args, "--goal").value_or("");
    while (plan.goal.empty()) {
        plan.goal = focus::promptLine("What will you get done? (be specific)");
        if (plan.goal.empty() && !std::cin) return 1;  // input closed
    }

    plan.approach = option(args, "--approach").value_or("");
    while (plan.approach.empty()) {
        plan.approach = focus::promptLine("What will you try on your own first, before asking AI?");
        if (plan.approach.empty() && !std::cin) return 1;
    }

    plan.duration = std::chrono::minutes(std::clamp(intOption(args, "--minutes").value_or(
                                                        static_cast<int>(config.focusDuration.count())),
                                                    1, 240));
    if (option(args, "--ai-budget").value_or("") == "none") {
        plan.aiBudget = std::nullopt;
    } else {
        plan.aiBudget = std::max(0, intOption(args, "--ai-budget").value_or(config.defaultAiBudget));
    }

    focus::Application app(db, config);
    return app.runSession(std::move(plan));
}

int commandStats(focus::Database& db, const Args& args) {
    const int days            = std::clamp(intOption(args, "--days").value_or(7), 1, 3650);
    const std::int64_t since  = focus::unixNow() - static_cast<std::int64_t>(days) * 24 * 3600;

    focus::SessionRepository sessions(db);
    focus::CheckInRepository checkIns(db);
    focus::printReport(focus::summarize(sessions.startedSince(since), checkIns.createdSince(since)), days);
    return 0;
}

int run(const Args& args) {
    if (args.empty() || args[0] == "help" || args[0] == "--help") {
        printUsage();
        return 0;
    }

    const std::string dir = focus::win::appDataDirectory();
    if (dir.empty()) {
        std::cerr << "Could not locate or create the data folder in %LOCALAPPDATA%.\n";
        return 1;
    }
    auto db = focus::Database::open(dir + "\\focus.db");
    focus::migrate(db);

    if (args[0] == "start") return commandStart(db, args);
    if (args[0] == "stats") return commandStats(db, args);

    printUsage();
    return 1;
}

}  // namespace

// wmain gives us the command line as UTF-16, so non-ASCII text in --goal survives intact.
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);  // print and read UTF-8 in the console
    SetConsoleCP(CP_UTF8);

    Args args;
    for (int i = 1; i < argc; ++i) args.push_back(focus::win::toUtf8(argv[i]));

    try {
        return run(args);
    } catch (const focus::StorageError& e) {
        std::cerr << "Storage error: " << e.what() << "\n";
        return 2;
    }
}
