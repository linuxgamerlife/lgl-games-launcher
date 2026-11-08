# 03 — Architecture

Status: draft for approval. Target: GTK 4 / gtkmm 4.20+, libadwaita 1.6+, C++20, Fedora 44.

This document describes the native GTK architecture for **Fedora with KineticWE + Noctalia**.
It deliberately keeps the proven separation in upstream Omakade — domain logic, services and
presentation — while replacing Qt types, QML, Omarchy theming and Hyprland assumptions with
the target desktop's APIs.

---

## 1. Principles

1. **Core is widget-free.** No file in `src/core/` may include a GTK, GDK, Gtkmm, Gdkmm or
   libadwaita header. Core logic builds into a static library and is testable without a
   display.
2. **Security boundaries are explicit.** Every untrusted input crosses a named validator.
   See [09-Security](09-SECURITY.md).
3. **One writer per store.** The GUI and `sessiond` share the SQLite database but each uses
   its own connection and WAL/busy-timeout rules; long work never holds the UI thread.
4. **No shell, ever.** All external programs use argv.
5. **Async by default.** Scans, network, image decode, archive work and save copies run off
   the main loop.
6. **Adapters, not assumptions.** Compositor, audio, notification, recorder and portal
   features sit behind interfaces with a `Noop`/`Unavailable` implementation.
7. **Compatibility is a feature.** The SQLite schema and `.omakade-backup` format are
   contracts with existing users.
8. **The repository root is the GTK app.** No `gtk/` wrapper and no active Qt tree. See
   [15-Migration](15-MIGRATION.md).

---

## 2. Process model

```text
                    ┌──────────────────────────────────────────────┐
                    │ lgl-games-launcher (GUI, unprivileged)        │
                    │                                              │
  launcher files ──►│ scanners ──► core library ──► Gio::ListModel │
  ROM folders    ──►│      │             │              │          │
  network        ──►│ metadata clients   │              ▼          │
                    │      │             │        gtkmm windows    │
  Secret Service ◄─►│ secrets            │              │          │
                    │      │             ▼              │          │
                    │      └──────► SQLite library ◄───┘          │
                    │                    ▲                          │
                    └────────────────────┼──────────────────────────┘
                                         │ WAL, separate connection
                    ┌────────────────────┴──────────────────────────┐
                    │ lgl-games-launcher-sessiond                    │
                    │ process polling, play sessions, Discord IPC,  │
                    │ optional highlight work                       │
                    └────────────────────┬──────────────────────────┘
                                         │ local Unix socket (0600)
                    ┌────────────────────┴──────────────────────────┐
                    │ lgl-games-launcher-guide-button (optional)     │
                    │ evdev read-only, controller Home/Capture      │
                    └───────────────────────────────────────────────┘
```

### Binaries

| Binary | Runs as | Purpose | Required |
| --- | --- | --- | --- |
| `lgl-games-launcher` | user | GTK application, library UI, launches games, owns single-instance socket | yes |
| `lgl-games-launcher-sessiond` | user | Play-session recorder, optional Discord presence, optional highlight work | runtime feature |
| `lgl-games-launcher-guide-button` | user | Reads controller Home/Capture evdev events and forwards short commands; no overlay | optional |

The in-game guide overlay is not built on KineticWE because the compositor does not provide
layer-shell. The guide-button helper can still forward Game Mode/desktop actions, but it must
not claim to open a guide.

### Why a separate sessiond

- Recording must continue while the library window is closed.
- A long-running recorder must not hold a write transaction that stalls the UI.
- The existing upstream design already separates it and has durable recovery behaviour.
- It can be restarted independently after a crash or restore.

The daemon is a systemd user service, started at graphical login (or on demand) and
stoppable without closing the library. It uses the same schema and a cooperative lock.

---

## 3. Layers

```text
src/
  main.cpp                 application lifecycle only
  core/                    no widgets; static library lgl_games_core
    paths/                 XDG paths and platform root discovery
    settings/              typed settings + migration
    db/                    sqlite3 RAII, migrations, transactions
    library/               source models, unified model, filters, organisation
    sources/               one directory per launcher/emulator
    launch/                launch plan, argv, environment, setup overrides
    tracking/              sessions, recording, statistics, baselines
    backup/                library backup and recovery
    saves/                 save layouts, snapshots, restore journals
    metadata/              IGDB, ProtonDB, artwork providers
    achievements/          Steam, RetroAchievements, Xbox
    text/                  bounded JSON/TOML/INI/VDF/zip helpers
    model/                 typed domain objects and value types
  platform/                GLib/Gio only; no widgets
    subprocess/            Gio::Subprocess wrappers and timeouts
    network/               libsoup 3 client, TLS, limits
    secrets/               libsecret C API
    notify/                GNotification / portal
    power/                 idle inhibit, suspend awareness
    compositor/            adapter interface + KineticWE/KWin/wlroots/noop
    shell/                 Noctalia IPC adapter (DND, theme, diagnostics)
    audio/                 PipeWire/pactl adapter
    controller/            SDL 3 polling, focus contract
    ipc/                   single-instance socket, controller helper protocol
    desktop/               Flatpak exports, desktop entries, XDG dirs
  ui/                      widgets and windows; static library lgl_games_ui
    window/                main window, title bar, menu, notices
    pages/                 home, library, details, stats, settings
    components/            cards, rows, chips, editors, empty states
    dialogs/               about, help, file, alert, editors
    themewatcher/          style loading and live reload
    gamepad/               focus engine and controller hints
  services/
    sessiond/              daemon main
    guidebutton/           evdev helper main (Home/Capture actions; no overlay)
  tests/                   g_test executables and CTest scripts (top-level tests/)
data/                      GResource: CSS, icons, console art, catalogs
packaging/                 spec, desktop, metainfo, icons, systemd units
docs/                      this documentation
legacy/qt/                 read-only upstream reference (not built)
```

Dependency direction is one-way:

```text
ui ──► core
ui ──► platform
services ──► core
services ──► platform
platform ──► core (types and paths only)
core ──► std + glibmm/giomm (utilities, no widgets)
```

`core` must not depend on `ui` or `platform` implementation details. If a core module needs
to run a program or read a URL, it accepts an interface (for example `ProcessRunner`,
`HttpClient`) that `platform` implements and tests fake.

---

## 4. Domain model

Use typed value objects rather than `QVariantMap`. Proposed types in `src/core/model/`:

| Type | Fields (minimum) |
| --- | --- |
| `SourceId` | stable enum/string: `steam`, `heroic`, `lutris`, `gog`, `retroarch`, `pcsx2`, … |
| `GameKey` | `source`, `runner`, `app_id`, `path` identity tuple |
| `Installation` | key, title, source, runner, app id, install path, launch path, flatpak, available |
| `Game` | canonical title, sort title, installations, preferred installation, metadata id, artwork ids, flags |
| `Artwork` | kind (`cover`, `hero`, `logo`, `icon`, `banner`), source URL/path, cache path, hash |
| `Collection`, `Tag`, `SavedFilter` | id, name, ordered membership/state |
| `PlaySession` | stable key, game key, start/end, seconds, intervals, provenance |
| `SaveSet` | layout id, version, manifest, files, hashes, created time |
| `LaunchPlan` | program, argv, environment, working dir, flatpak flag, expected process identities |
| `ConnectionStatus` | available, last error, last scan, counts |

Rules:

- A game key is stable across scans and launches.
- Paths are stored as absolute paths in the local machine; they are not promised portable.
- Display titles are never used as identity.
- `Game` may have several installations; exactly one is preferred for Play.
- All user-visible text is escaped by GTK; no markup is built from untrusted strings.
- Parser output is validated before it becomes a domain object.

The GTK view model wraps `Game` in a `Glib::Object` with read-only properties so
`Gtk::GridView` factories can bind to it without copying strings on every cell.

---

## 5. Library data flow

### 5.1 Scan

```text
source adapter
  discover roots (PATH, XDG, Flatpak exports)
  read launcher files with size caps
  parse into raw records
  validate fields and paths
  upsert source cache in SQLite (single transaction per source)
  emit source-changed
        │
        ▼
UnifiedLibrary (core)
  merge installations by identity
  apply user links / preferred installation
  apply hidden / favourite / collection / tag state
  build rows
        │
        ▼
Gio::ListModel
  filter (source, installed, console, search, saved filter)
  sort (title, recent, playtime, rating, popularity, installed)
        │
        ▼
Gtk::GridView / Gtk::ListView
```

A scan never deletes user organisation. Removing a source only hides its rows; cached rows
remain until a deliberate cleanup.

### 5.2 Search

Search is a core function over normalised title, aliases and source title. It runs over the
in-memory row set, not the database, so typing never blocks on I/O. With 10,000 rows the
target is under 50 ms per keystroke; if that is missed, move to a precomputed token index in
core.

### 5.3 Artwork

1. Check user override in `artwork_overrides`.
2. Check local launcher artwork.
3. Check cache by content hash.
4. Optionally fetch from an enabled provider (off by default for network providers).
5. Decode off the main thread into a `Gdk::Texture`.
6. Bound the cache by count and bytes; evict least-recently used.

Never decode an unbounded image on the UI thread. Reject images over the configured pixel
and byte limits. See `SEC-ART-*` in [09-Security](09-SECURITY.md).

---

## 6. Launch pipeline

```text
UI Play
  → resolve Game and preferred Installation
  → LaunchInspector validates content and setup
  → LaunchPlan (program, argv, env, cwd, detached)
  → confirm destructive or unusual choices if required
  → Gio::Subprocess::launch (argv, no shell)
  → return launch result + expected process identity
  → sessiond observes the process and records the session
```

Rules:

- Program resolution uses an explicit list of allowed executables for each source.
- Flatpak uses `flatpak run <validated-app-id> -- <validated-args>`.
- Emulator arguments come from a typed `LaunchPlan`, not string concatenation.
- User manual commands are split with a documented shell-like parser **without invoking a
  shell**, then validated; they are labelled as user-supplied and never constructed from
  network data.
- Environment is an allowlist plus documented source-specific variables. Credentials are
  never placed in argv.
- Working directory is validated and must exist.
- A launch failure is shown as a notice with the exit status and the first bounded lines of
  stderr; no dialog unless the user must decide.
- The process is detached; the GUI does not wait for the game.
- `sessiond` attributes the session by process identity, path and, where available, a
  compositor window title.

---

## 7. Session tracking

```text
process poll (every 5 s, worker thread in sessiond)
  /proc scan + helper wrappers
  match known emulator/game profiles
  open/extend/close sessions
  flush every interval with monotonic time
  optional focus predicate from compositor adapter
  optional Discord presence
        │
        ▼
SQLite: play_sessions, play_session_intervals, play_baselines,
        session_journal_state, session_tombstones
```

- Time is monotonic; wall time is only for placement.
- A durable journal beside the database replays refused writes after a restart.
- A cooperative lock prevents two recorders from owning the same library.
- Restore refuses while the recorder owns the lock.
- The GUI reads totals; it never writes session rows.

---

## 8. Threading and async

| Work | Where it runs | Result delivery |
| --- | --- | --- |
| Source scan | worker thread | `Glib::Dispatcher` → update model |
| Network request | libsoup async on main context, parsing on worker | callback with generation check |
| Image decode | `GThreadPool` | `Glib::Dispatcher` → texture |
| Archive/save copy | worker | dispatcher |
| Session poll | sessiond worker | database + optional socket |
| Controller poll | main loop source at fixed rate | direct state update |
| Theme reload | main loop, debounced 200 ms | signal |

Rules:

- No file, network, archive, image or process work on the GTK main loop.
- Every async request carries a generation counter; stale results are discarded.
- Shared mutable state is guarded; prefer message passing to locks.
- Cancellation is explicit on window close, source disable and restore.
- Worker threads never touch widgets.

---

## 9. IPC and command line

### Single instance

- `Gio::SocketService` on `$XDG_RUNTIME_DIR/lgl-games-launcher/control.sock`.
- Directory mode `0700`, socket mode `0600`.
- Protocol: newline-delimited JSON, maximum 4 KiB per line, maximum 8 KiB total.
- Commands: `activate`, `play <key>`, `rescan <source>`, `game-mode enter|exit|toggle`,
  `fullscreen toggle`, `quit`, `clips changed`.
- Unknown commands are rejected and logged once.
- A second launch with no server starts normally.

### Command line

Preserve the useful upstream commands, with documented parsing and validation:

```text
lgl-games-launcher [--demo] [--play SOURCE:RUNNER:ID] [--quit]
                  [--game-mode] [--game-mode-exit] [--game-mode-toggle]
                  [--couch] [--clip] [--help] [--version]
```

- `--render-screenshot`/`--render-size` and test-only flags are replaced by the standard
  snapshot mechanism (see [12-Testing](12-TESTING.md)).
- Options that need a value reject a missing value with exit code 2 and a usage message.
- No command-line flag can execute an arbitrary program.

---

## 10. Error handling

- Recoverable domain errors use `std::expected<T, Error>` (C++23) or a project `Result<T>`
  shim on C++20. Do not mix models in one subsystem.
- gtkmm APIs throw `Glib::Error`; catch at the UI/action boundary, convert to a domain error
  or a user notice, and never let an exception escape a callback.
- Exceptions are not used for normal control flow.
- Every external program result includes exit status, signal and bounded stderr.
- Every network result includes HTTP status, content type, size and a bounded error string.
- Errors are logged without secrets or full personal paths unless needed for diagnosis; a
  user-visible message never includes a token.
- A failed optional feature degrades; it does not crash the library.

---

## 11. Logging

- Use `g_message`/`g_warning`/`g_critical` with a project log domain
  (`lgl-games-launcher`).
- `G_MESSAGES_DEBUG=all` enables verbose logs in development.
- Never log secrets, API keys, credentials, full save contents, ROM hashes or file bytes.
- Log one line per privileged-equivalent action: launch requested, program + argv with
  secrets removed, result; scan start/end/counts; restore start/result; network host and
  status (no query strings that contain credentials).
- journald captures stdout/stderr for the daemon; the GUI logs to stderr.

---

## 12. Settings and storage

- Settings: one `Settings` class in `src/core/settings/` with typed getters/setters and an
  injectable path. `Glib::KeyFile`; every setter saves immediately; malformed values fall
  back to defaults.
- Database: one wrapper around `sqlite3` in `src/core/db/`, prepared statements only,
  foreign keys on, WAL, `busy_timeout = 5000`, `synchronous = NORMAL`.
- Atomic writes use a sibling temporary file, `fsync` where required, then `rename`.
- Directories are created with mode `0700`; files with `0600` unless they are exported to the
  user.
- The database schema and compatibility rules are in [04-Data model](04-DATA-MODEL.md).

---

## 13. Platform adapters

Interfaces in `src/platform/` with real and no-op implementations:

| Interface | Real implementations | Fallback |
| --- | --- | --- |
| `Compositor` | KineticWE: `org.kde.KWin` + `org.kde.kwin.Scripting` + `kde-output-management-v2`; wlroots foreign-toplevel | `UnavailableCompositor` |
| `Shell` | Noctalia IPC: `notification-dnd-status`, `notification-dnd-set`, `notification-dnd-toggle`; theme change probe | `NoopShell` |
| `AudioRouter` | PipeWire (`wpctl`) or `pactl` | keep current output |
| `NotificationControl` | Noctalia DND IPC | leave notifications alone |
| `GlobalShortcuts` | KGlobalAccel D-Bus; XDG Global Shortcuts portal | Kinetic Settings documentation |
| `IdleInhibitor` | `Gtk::Application::inhibit`, KineticWE idle-inhibit | no-op |
| `ScreenRecorder` | KineticWE ScreenCast portal / external recorder adapter (P3) | highlights unavailable |
| `Overlay` | Not available: KineticWE does not provide layer-shell | no overlay |
| `SecretStore` | libsecret | memory-only for the session, with a warning |
| `ProcessRunner` | `Gio::Subprocess` | none (hard failure) |
| `HttpClient` | libsoup 3 | offline |

Adapters are selected at startup by probing capabilities, not by distribution name. A
capability report is available in Settings → Diagnostics.

### 13.1 KineticWE compositor adapter

KineticWE is a KWin fork, so the adapter uses the same D-Bus names KDE applications use. It
must probe each capability and never assume a method exists.

- **Outputs:** `kde-output-management-v2` or the `org.kde.KWin` output D-Bus. Enumerate
  connector, make/model, enabled state and current mode. Changing an output is a privileged
  user action and requires confirmation when it would disable the display the user is on.
- **Windows:** KWin scripting is the only public way to move windows between outputs and
  virtual desktops. Ship a small, read-only JavaScript file in the GResource, load it through
  `org.kde.kwin.Scripting` by absolute path, and ask it to perform one allow-listed operation
  at a time. Never accept script source from the database, network or user input.
- **Workspaces:** `org.kde.KWin.VirtualDesktopManager` for listing and switching.
- **Focus / fullscreen:** expose one operation per need (place, return, hold, focus, set mode)
  with validated window UUID/address and workspace identifiers.
- **Placeholder:** a window with a known title and class, as upstream, so a failed move can be
  detected and undone.
- **State journal:** write every change to disk before applying it, so an interrupted session
  can be restored on next start, exactly like the upstream design.

The script is a security boundary. It is treated like a privileged helper even though it runs
as the user: allow-listed functions only, no string interpolation of untrusted data, no
arbitrary `callDBus`, and a bounded operation queue. See `SEC-COMPOSITOR-*` in
[09-Security](09-SECURITY.md).

### 13.2 Noctalia shell adapter

Noctalia owns notifications, DND, the launcher, OSDs and the GTK theme generation.

- **Notifications:** use `GNotification`; Noctalia is the `org.freedesktop.Notifications`
  owner in the reference session.
- **DND:** invoke `noctalia-kwe msg notification-dnd-status` and parse a boolean; save the
  previous value, set DND on entering Game Mode, restore it on exit. If any call fails, log a
  note and leave notifications unchanged.
- **Theme:** Noctalia's palette/template system writes the GTK 3/4 theme and color scheme. The
  app follows GTK named colours and the ThemeWatcher reloads on change. The app must not read
  Noctalia's private config or write to it.
- **Launcher:** a normal `.desktop` file with the app icon and a Game Mode action is enough;
  Noctalia's launcher scans XDG entries.
- **Shortcuts:** register through KGlobalAccel or the GlobalShortcuts portal. Do not ship a
  Noctalia Luau plugin in 1.0; Noctalia plugins are trusted code and out of scope.
- **IPC safety:** the socket is per-user; commands are fixed strings; output is read with a
  timeout and a size cap. No command is constructed from game titles or paths.

### 13.3 Guide overlay — not available

The in-game guide is **not part of the architecture**. KineticWE does not provide
`zwlr_layer_shell_v1`, so a GTK 4 layer-shell overlay cannot be created above a fullscreen game.

Consequences:

- `gtk4-layer-shell` is not a build or runtime dependency.
- No guide process is built for the target.
- The UI reports the guide as unavailable; a normal fullscreen window is not a substitute,
  because it can steal input from the game.
- If KineticWE later exposes a supported overlay surface, this decision is revisited and a new
  security review is required.

The guide data protocol may be specified for that future, but no overlay code ships now.



---

## 14. GTK application structure

Following the GTK standard:

- `main.cpp` owns only `Gtk::Application`, `startup`, `activate` and the window lifetime.
- The main window is `gameslauncher::Window`, a `Gtk::ApplicationWindow`.
- The header bar is a member, not `make_managed`, so it can move between title bar modes.
- The body is a `Gtk::Box` inside a `Gtk::Overlay` for notices.
- Pages are `Gtk::Stack` children; the library grid uses `Gtk::GridView` with a
  `Gtk::SingleSelection` and a `Gtk::FilterListModel`/`Gtk::SortListModel` chain.
- Settings and dialogs are `Gtk::Window` subclasses, not Adwaita widgets.
- libadwaita is used only for `adw_init()` and `AdwStyleManager`, as the standard requires.
- CSS, icons, console art and catalogues are in a GResource.
- `ThemeWatcher` lives in `src/ui/themewatcher/` with pure helpers in
  `src/core/theme/`.

The full screen specification is in [05-UI specification](05-UI-SPEC.md).

---

## 15. Build targets

```cmake
add_library(lgl_games_core STATIC ...)   # no widgets; links glibmm/giomm/json-glib/sqlite3
add_library(lgl_games_platform STATIC ...) # Gio, libsoup, libsecret, SDL3, SQLite wrappers
add_library(lgl_games_ui STATIC ...)     # gtkmm, gdkmm, libadwaita C API
add_executable(lgl-games-launcher ...)    # main + resources; links all three
add_executable(lgl-games-launcher-sessiond ...)
add_executable(lgl-games-launcher-guide-button ...) # optional, if built
```

- Warnings as errors on C++ (`-Wall -Wextra -Wpedantic -Werror`); generated resource C with
  `-w`.
- `LGL_GAMES_LAUNCHER_ENABLE_SANITIZERS` for ASan+UBSan.
- `LGL_GAMES_LAUNCHER_VERSION` compile definition.
- One test executable per core module, plus UI/snapshot tests.
- Install rules in the root CMake, per the migration standard.

---

## 16. What is deliberately not copied

- QML binding expressions and anchors: rebuilt as GTK layout and explicit signal handlers.
- `OmarchyTheme`: replaced by Noctalia-generated GTK theme, named colours and ThemeWatcher.
- Hyprland `hyprctl` scripts: replaced by the KineticWE/KWin adapter and a bounded bundled
  KWin script.
- Qt `QVariant` maps crossing every boundary: replaced by typed structs.
- Test-only flags embedded in `main.cpp`: replaced by test executables and the standard
  snapshot environment variable.
- A single 9,486-line `main.cpp`: split into application, window, pages and services.

The upstream code remains valuable as a specification of behaviour. It is not a template for
file structure.
