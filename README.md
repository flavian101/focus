# Focus

A native Windows (C++20) app that helps you **focus**, notice how you **feel**, and keep your
**AI use deliberate** instead of automatic.

Each session is a small contract with yourself:

1. Say *what* you will get done and *what you will try on your own first*.
2. Do a 10-second mood/energy check-in.
3. Work. Focus watches which kind of window is in front (editor, distraction, AI assistant),
   counts AI visits against your budget, and nudges you with a notification when you go over.
4. Check in again. Everything is saved locally, and `focus stats` shows trends.

Everything stays on your machine (`%LOCALAPPDATA%\Focus\focus.db`). Window titles are used to
decide a category and are then discarded; only category totals are stored.

## Build (Windows)

Requirements: Visual Studio 2022 (Desktop development with C++) and CMake 3.24+.

```
cmake --preset vs2022
cmake --build --preset vs2022-debug
ctest --preset vs2022-debug
build\vs2022\src\app\Debug\focus.exe start
```

If your network blocks sqlite.org, install SQLite another way (e.g. vcpkg) and configure with
`-DFOCUS_FETCH_SQLITE=OFF`.

## Use

```
focus start [--goal TEXT] [--approach TEXT] [--minutes N] [--ai-budget N|none]
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
- Scheduler (breaks, long breaks) and idle detection (don't count time away)
- Services from structure.md: Hydration, Stretch, Eye Care, Achievements
- Storage/Preferences: user-editable rules and defaults
- Weekly review: "what was I doing when my mood dropped?"
