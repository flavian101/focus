#include "app/Application.h"

#include <atomic>
#include <cstdio>
#include <iostream>

#include "app/Console.h"
#include "app/Report.h"
#include "app/TimeUtil.h"
#include "platform/WindowDetection.h"
#include "storage/CheckInRepository.h"
#include "storage/SessionRepository.h"

namespace focus {

namespace {

// Our own message numbers. WM_APP and above are reserved for application use.
constexpr UINT kMsgTray    = WM_APP + 1;  // mouse events on the tray icon (unused for now)
constexpr UINT kMsgAbandon = WM_APP + 2;  // "please stop" request, e.g. from Ctrl+C

constexpr UINT_PTR kTickTimerId = 1;
constexpr UINT kTickMillis      = 1000;

enum HotkeyId : int { kHotkeyPause = 1, kHotkeyLogAi = 2, kHotkeyAbandon = 3 };

const wchar_t kWindowClass[] = L"FocusHiddenMessageWindow";

// Ctrl+C runs on a different thread than the message loop, so it must not touch the session.
// It only posts a message, which is thread-safe, and lets the loop do the work.
std::atomic<HWND> g_window{nullptr};

BOOL WINAPI consoleCtrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        if (HWND w = g_window.load()) PostMessageW(w, kMsgAbandon, 0, 0);
        return TRUE;  // handled: don't kill the process, let us save the session first
    }
    return FALSE;
}

std::string formatClock(std::chrono::seconds s) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld", static_cast<long long>(s.count() / 60),
                  static_cast<long long>(s.count() % 60));
    return buf;
}

}  // namespace

Application::Application(Database& db, Configuration config)
    : db_(db),
      config_(config),
      session_([this](const SessionEvent& e) { onSessionEvent(e); }) {}

Application::~Application() {
    hotkeys_.reset();  // unregister hotkeys and remove the tray icon BEFORE the window goes away
    tray_.reset();
    if (hwnd_) DestroyWindow(hwnd_);
}

bool Application::createWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &Application::windowProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    // Never shown: it exists only so Windows has somewhere to deliver messages.
    // `this` travels through the last argument so windowProc can find us again.
    hwnd_ = CreateWindowExW(0, kWindowClass, L"Focus", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                            wc.hInstance, this);
    return hwnd_ != nullptr;
}

LRESULT CALLBACK Application::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Windows calls this plain function; we need our object. Stash the pointer on the window.
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Application::handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            if (wParam == kTickTimerId) onTick();
            return 0;
        case WM_HOTKEY:
            onHotkey(static_cast<int>(wParam));
            return 0;
        case kMsgAbandon:
            session_.abandon(SessionManager::Clock::now());
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void Application::onTick() {
    const auto now = SessionManager::Clock::now();

    if (now - lastSampleAt_ >= config_.activitySampleInterval) {
        lastSampleAt_ = now;
        const auto foreground = win::queryForegroundWindow();
        // Only the category is kept. The window title is dropped right here.
        session_.recordActivity(foreground ? classifier_.classify(*foreground) : ActivityCategory::Neutral, now);
    }
    session_.tick(now);  // may complete the session, which quits the message loop

    if (session_.isActive() && tray_) tray_->setTooltip(tooltip());
}

void Application::onHotkey(int id) {
    const auto now = SessionManager::Clock::now();
    switch (id) {
        case kHotkeyPause:   togglePause(); break;
        case kHotkeyLogAi:   session_.logManualAiUse(now); break;
        case kHotkeyAbandon: session_.abandon(now); break;
    }
}

void Application::togglePause() {
    const auto now = SessionManager::Clock::now();
    if (session_.state() == SessionState::Focusing) session_.pause(now);
    else if (session_.state() == SessionState::Paused) session_.resume(now);
}

std::string Application::tooltip() const {
    const auto& s = session_.current();
    std::string text = "Focus " + formatClock(session_.remaining(SessionManager::Clock::now())) + " left";
    if (s.aiBudget) text += " | AI " + std::to_string(s.aiTotal()) + "/" + std::to_string(*s.aiBudget);
    return text;
}

void Application::onSessionEvent(const SessionEvent& event) {
    using Type    = SessionEvent::Type;
    const auto& s = session_.current();
    auto notify   = [this](const std::string& title, const std::string& body) {
        if (tray_) tray_->notify(title, body);
    };

    switch (event.type) {
        case Type::Started:
            notify("Focus started", s.goal);
            break;
        case Type::Paused:
            std::cout << "  paused\n";
            break;
        case Type::Resumed:
            std::cout << "  resumed\n";
            break;
        case Type::AiVisit: {
            std::string count = std::to_string(s.aiTotal());
            if (s.aiBudget) count += " of " + std::to_string(*s.aiBudget);
            notify("AI visit " + count, "Be specific: what is the exact question, and what did you already try?");
            break;
        }
        case Type::AiBudgetExceeded:
            notify("AI budget used up",
                   "Write down what you've tried. Is this a real blocker, or are you avoiding the hard part?");
            break;
        case Type::Completed:
            notify("Session complete", "Nice work. Take a short break.");
            PostQuitMessage(0);  // ends the message loop in runSession()
            break;
        case Type::Abandoned:
            PostQuitMessage(0);
            break;
    }
}

void Application::save(const CheckIn& before, const CheckIn& after) {
    SessionRepository sessions(db_);
    CheckInRepository checkIns(db_);

    Transaction tx(db_);  // session + both check-ins are saved together, or not at all
    const auto id = sessions.insert(session_.current());

    CheckIn b = before, a = after;
    b.sessionId = id;
    a.sessionId = id;
    checkIns.insert(b);
    checkIns.insert(a);
    tx.commit();
}

int Application::runSession(SessionPlan plan) {
    std::cout << "\nGoal:      " << plan.goal << "\n"
              << "First try: " << plan.approach << "\n"
              << "Length:    " << plan.duration.count() << " min";
    if (plan.aiBudget) std::cout << "   AI budget: " << *plan.aiBudget << " visit(s)";
    std::cout << "\n";

    const CheckIn before = promptCheckIn(CheckInKind::BeforeSession);

    if (!createWindow()) {
        std::cerr << "Could not create the message window.\n";
        return 1;
    }
    g_window.store(hwnd_);
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    tray_ = std::make_unique<win::TrayIcon>(hwnd_, kMsgTray);
    tray_->add("Focus");

    hotkeys_ = std::make_unique<win::HotkeyManager>(hwnd_);
    const bool hotkeysOk = hotkeys_->add(kHotkeyPause, MOD_CONTROL | MOD_ALT, 'P') &&
                           hotkeys_->add(kHotkeyLogAi, MOD_CONTROL | MOD_ALT, 'A') &&
                           hotkeys_->add(kHotkeyAbandon, MOD_CONTROL | MOD_ALT, 'Q');
    if (!hotkeysOk) std::cout << "Warning: some hotkeys are taken by another program and will not work.\n";

    const auto now = SessionManager::Clock::now();
    lastSampleAt_  = now;
    if (!session_.start(plan, now)) {
        std::cerr << "Could not start the session (is the goal empty?).\n";
        return 1;
    }
    SetTimer(hwnd_, kTickTimerId, kTickMillis, nullptr);

    std::cout << "\nFocusing. Hotkeys: Ctrl+Alt+P pause/resume, Ctrl+Alt+A log an AI use, Ctrl+Alt+Q abandon.\n"
              << "Ctrl+C also ends the session (it is saved).\n";

    // The message loop: sleep until Windows hands us a message, dispatch it, repeat.
    // PostQuitMessage() makes GetMessage return 0, which ends the loop.
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(hwnd_, kTickTimerId);
    SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
    g_window.store(nullptr);

    printSessionSummary(session_.current());
    const CheckIn after = promptCheckIn(CheckInKind::AfterSession);
    save(before, after);
    std::cout << "\nSaved. Run 'focus stats' to see your trends.\n";
    return 0;
}

}  // namespace focus
