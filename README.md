# Focus

A native Windows (C++20) app that helps you **focus**, notice how you **feel**, and keep your
**AI use deliberate** instead of automatic.

Each session is a small contract with yourself:

1. Say *what* you will get done and *what you will try on your own first*.
2. Do a 10-second mood/energy check-in.
3. Work. Focus watches which kind of window is in front (editor, distraction, AI assistant),
   counts AI visits against your budget, and nudges you with a notification when you go over.
   Time with no keyboard/mouse input for 5 minutes is counted as "away", not focus, and
   Eye Care (every 20 min) and Stretch (45 min) reminders pop up while you work. Water and meals are
   handled all day by the Qt app (see below).
4. Check in again. Everything is saved locally, and `focus stats` shows trends.

Everything stays on your machine (`%LOCALAPPDATA%\Focus\focus.db`). Window titles are used to
decide a category and are then discarded; what is stored is category totals, app names (depending on
the tracking level) and plain numbers about how the keyboard and mouse were used.

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

## The Qt window (optional)

`focus_ui` is a Qt Quick front end. It uses the same database as the console app.

- **Today**: plan your day. Add tasks with a priority (Must / Should / Could) and a time estimate;
  you get at most 3 Musts, because if everything is a must, nothing is. Tick tasks off, drop them,
  or press Focus to start a session on one: its time is added to the task, and the check-in offers
  to mark it done. Unfinished tasks from earlier days can be carried over in one click.
- **Health**: water and meals, all day while Focus runs in the tray (even with the window closed).
  Water is a *pace*, not a timer: pick a daily goal and you are nudged only when you fall behind the
  pace for your waking day, never right after logging a glass. Meals use windows (breakfast, lunch,
  dinner): you are reminded when a window opens and every 30 minutes until you tap "I ate" or "Skip".
  Reminders are held back while you are away from the keyboard and appear when you return.
  The tray menu has "Log a glass of water".
- **Activity**: what your computer day looked like. While Focus runs in the tray it records, minute
  by minute, which *category* (focus / AI / other / distraction / away) and which *app name* was in
  front. You see an hour-by-hour stacked chart, your most used apps, your longest focus block, and how
  often you switch apps. Time away from the keyboard is counted as away, and time the laptop slept
  is not counted at all. **Hands-on focus** is the share of your focus time that had real keyboard or
  mouse input, which tells "the editor was open" apart from "I was working in it"; it also notes how
  often the window or tab changed. **Privacy:** what is stored, on this computer, is app names,
  categories and numbers: seconds with input, how often the window changed and, in the highest
  level only, how many scrolls, key presses and clicks. Never window titles, never *which* keys, never
  what you type (the counters decide from the kind of event alone and do not read the key or the
  pointer position). You can pause tracking or delete today's data from the same page.
- **Settings** (top right): pick an accent colour and theme, write a few words about you and what this
  computer is for, and choose how much detail tracking keeps. *Work hours* (days and times you set) and
  *outside work hours* each have a mode: **Full + counts** is Full detail plus the scroll, key and
  click counts (the only level that listens for them at all; nothing is installed in any other level),
  **Full detail** keeps the app name (code, chrome), **Category only** keeps just "40 minutes of
  distraction" and never which app, **Off** records nothing. The default is full detail during work
  and category only outside it. The tray icon's tooltip shows the
  mode that is active right now.
- On the Activity tab you can **click an app to correct its category** ("this app is Focus"). The
  correction applies from then on and re-labels today's recorded time. For a browser it only changes
  the default, so an AI chat or a video feed in a tab is still recognised.
- **Insights**: animated summary cards and a weekly chart, with recent sessions and AI use.
- A live session screen with a progress ring and AI-budget tracker, a check-in sheet, a tray icon,
  and light/dark themes with a choice of accent colours.

1. Install Qt 6 with the Qt Online Installer and tick **Qt 6.x > MSVC 2022 64-bit**
   (that kit works with Visual Studio 2026).
2. Build with the UI switched on, pointing CMake at your Qt kit (adjust the version and path):

```
cmake --preset vs2026 -DFOCUS_BUILD_UI=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build --preset vs2026-release
build\vs2026\src\ui\Release\focus_ui.exe
```

The build runs `windeployqt` so the exe starts without Qt on your PATH. `ctest --preset vs2026-debug`
also runs `ui_self_test`, which loads the real QML, plays through a session and writes screenshots to
`build\vs2026\ui-screenshots`.

If a rebuild fails with `LNK1104: cannot open file '...focus_ui.exe'`, the app is still running
(closing its window keeps it in the tray so a session is not lost). Quit it from the tray icon,
or run `Get-Process focus_ui | Stop-Process`, then build again.

## Run the console app

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

- Editable meal times and waking hours (a settings page)
- A daily review built on the activity data, then an optional local-model summary
- Global hotkeys in the Qt window (the tray menu covers them for now), a settings page, custom icon
- Break flow after a session (short/long breaks) on top of the Scheduler
- Achievements, and reminders that run all day (needs a background mode)
- Storage/Preferences: user-editable rules and defaults
- Weekly review: "what was I doing when my mood dropped?"
