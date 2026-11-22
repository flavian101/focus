# Focus

A native Windows (C++20) app that helps you **focus**, notice how you **feel**, and keep your
**AI use deliberate** instead of automatic.

Each session is a small contract with yourself:

1. Say *what* you will get done and *what you will try on your own first*.
2. Do a 10-second mood/energy check-in.
3. Work. Focus watches which kind of window is in front (editor, distraction, AI assistant),
   counts AI visits against your budget, and nudges you with a notification when you go over.
   Time with no keyboard/mouse input for 5 minutes is counted as "away", not focus, and
   Eye Care (every 20 min), Stretch (45 min) and Water (60 min) reminders pop up while you work.
4. Check in again. Everything is saved locally, and `focus stats` shows trends.

Everything stays on your machine (`%LOCALAPPDATA%\Focus\focus.db`). Window titles are used to
decide a category and are then discarded; only category totals are stored.

## Build (Windows)

Requirements: Visual Studio 2026 (Desktop development with C++) and **CMake 4.2 or newer**
(the first version that knows the "Visual Studio 18 2026" generator; check with `cmake --version`).

```
cmake --preset vs2026
cmake --build --preset vs2026-debug
ctest --preset vs2026-debug
build\vs2026\src\app\Debug\focus.exe start
```

On Visual Studio 2022, use the `vs2022` presets instead. If your CMake is older than 4.2, either
install a newer one from cmake.org or use the `ninja` preset from an
"x64 Native Tools" developer prompt.

If your network blocks sqlite.org, install SQLite another way (e.g. vcpkg) and configure with
`-DFOCUS_FETCH_SQLITE=OFF`.

## Run it

Visual Studio: set `focus.exe` as the startup item and press F5. With no arguments it asks for
your goal interactively (same as `focus start`).

PowerShell does not run programs from the current folder by name, so from the build folder use
`.\focus.exe start`. To run `focus` from anywhere, install it and put it on your PATH:

```
cmake --install build\vs2026 --config Release --prefix "$env:LOCALAPPDATA\Programs\Focus"
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";$env:LOCALAPPDATA\Programs\Focus\bin", "User")
```

Open a new terminal afterwards, then `focus start` works anywhere.

## Use

```
focus start [--goal TEXT] [--approach TEXT] [--minutes N] [--ai-budget N|none] [--no-reminders]
focus stats [--days N]
```

| Hotkey (global) | Action |
| --- | --- |
| Ctrl+Alt+P | pause / resume |
| Ctrl+Alt+A | log an AI use Focus can't see (e.g. AI inside your editor) |
| Ctrl+Alt+Q | abandon the session (it is still saved) |

## Layout

```
src/
  core/      Timer, SessionManager, CheckIn, Statistics, Configuration   (portable, no Win32)
  services/  ActivityClassifier (AI awareness)                           (portable)
  storage/   SQLite wrapper, migrations, repositories                    (portable)
  platform/  window detection, tray + notifications, hotkeys, paths      (Win32 only)
  app/       main, Application (message loop), console prompts, reports
tests/       doctest unit tests for everything portable
```

Rule of thumb: dependencies point downward. `core` knows nothing about SQLite or Win32,
which is why it can be unit-tested without a clock, a window, or a database.

## Roadmap

- Dashboard / Settings UI (Dear ImGui on Direct3D 11)
- Break flow after a session (short/long breaks) on top of the Scheduler
- Achievements, and reminders that run all day (needs a background mode)
- Storage/Preferences: user-editable rules and defaults
- Weekly review: "what was I doing when my mood dropped?"
