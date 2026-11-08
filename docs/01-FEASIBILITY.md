# 01 — Feasibility

**Question:** can Omakade 1.18.0 be rebuilt as a native GTK 4 application for Fedora
without losing the features that make it useful?

**Answer:** yes for the core product, conditionally for the desktop-specific features, and
no for a literal one-to-one port of Omarchy. The correct plan is a staged native rewrite
with explicit adapters and degradations, not a mechanical translation.

Date of audit: 2026-11-08. Upstream version: 1.18.0. Upstream licence: GPL-3.0-or-later,
Copyright (C) 2026 Tyler South.

---

## 1. What was audited

The whole upstream tree was inspected at `/development/projects/omarchy_plugins/omakade`:

| Area | Size |
| --- | --- |
| C++ sources and headers | 296 files, about 76,400 lines |
| QML and JavaScript | 44 files, about 19,800 lines |
| Tests (C++ and Python) | 28 files, about 31,600 lines |
| Bundled resources | about 86 MB, including 45 console portraits |
| Auxiliary programs | `omakade`, `omakade-sessiond`, `omakade-guide-button`, `omakade-guide-guard` |
| Build | CMake 3.24+, Qt 6.8+, Ninja, C++20 |
| Supported upstream desktop | Omarchy (Arch Linux, Hyprland, Wayland) |

The upstream architecture is already reasonably layered: `omakade_core` is a static library
with scanning, library, launch, tracking, backup, save, metadata and achievement logic;
`omakade_tracking` and `omakade_guide` are smaller static libraries shared with daemons; the
GUI is `main.cpp` plus QML. This makes a port feasible: the domain logic is separable from
the presentation even though it currently uses Qt types throughout.

---

## 2. Upstream dependency inventory

### 2.1 Build and runtime dependencies

| Upstream dependency | Used for | Fedora 44 availability |
| --- | --- | --- |
| Qt 6.8: Core, Gui, Network, Qml, Quick, Quick Controls 2, Sql, Test, Concurrent, DBus, GuiPrivate | Application, QML UI, SQLite, networking, IPC, tests, Wayland access | Available, but must be removed from the GTK port |
| LayerShellQt | Game Mode overlay and guide over a fullscreen game | Not available: KineticWE does not provide `zwlr_layer_shell_v1`; no replacement overlay |
| SDL 3 | Controller input | Available (`SDL3-devel 3.4.20`); recommended to keep |
| OpenSSL (Crypto) | Hashing and cryptographic helpers | Available; several uses can move to GChecksum |
| libzstd | Reading compressed archive formats | Available |
| libsecret | Secret Service credentials | Available |
| libzip | Backup archives and save sets | Available |
| wayland-client / wayland-protocols / wayland-scanner | Optional idle inhibit | Available; GTK has higher-level inhibition |
| Python 3 | Some upstream tests and tools | Available; the GTK test plan uses CTest and `g_test` for product code |

### 2.2 Fedora packages verified for the target

Checked against the Fedora 44 repositories on 2026-11-08:

| Package | Version found |
| --- | --- |
| `gtkmm4.0-devel` | 4.22.0 |
| `libadwaita-devel` | 1.9.4 |
| `json-glib-devel` | 1.10.8 |
| `sqlite-devel` | 3.51.2 |
| `libsecret-devel` | 0.21.8.2 |
| `libsoup3-devel` | 3.6.6 |
| ~~`gtk4-layer-shell-devel`~~ | 1.3.0 — **not used**; KineticWE lacks the protocol |
| `SDL3-devel` | 3.4.20 |
| `libmanette-devel` | 0.2.13 |
| `libportal-gtk4-devel` | 0.10.0 |
| `gstreamer1-devel` | 1.28.8 |

The GTK standard requires gtkmm 4.20+ and libadwaita 1.6+; Fedora 44 exceeds both. No
toolkit-version blocker exists.

### 2.3 External programs and services

| Upstream dependency | KineticWE/Noctalia situation | Port decision |
| --- | --- | --- |
| `hyprctl` | Hyprland only; KineticWE is a KWin fork with `org.kde.KWin` D-Bus, KWin scripting, KGlobalAccel, virtual desktops and output management | Replace with a KineticWE adapter; no GNOME fallback assumption |
| `omarchy-shell` | Not present; Noctalia owns notifications, DND, launcher, OSDs and shell surfaces | Drop the dependency; use Noctalia IPC where a shell action is needed |
| `gpu-screen-recorder` | Not in Fedora official repositories; KineticWE exposes ScreenCast through its portal | Optional external tool; highlights deferred |
| `ffmpeg` | Available in Fedora, but patent/codec packaging varies | Optional external tool; only for optional clips |
| Steam | RPM Fusion/Flatpak | Supported |
| Lutris | Fedora official 0.5.22 | Supported |
| Heroic | Flatpak (Flathub), not Fedora official | Supported by detection |
| Faugus | Not a Fedora package | Detect if present; no Fedora promise |
| RetroArch | Fedora official 1.22 | Supported |
| PCSX2, RPCS3, Dolphin, Cemu, melonDS, PPSSPP, xenia, shadPS4, Ryujinx/Eden, Kyty | Mostly Flatpak, some third-party RPM/COPR | Supported by generic discovery when installed |
| Sunshine | Flatpak/third-party | Optional, later phase |
| Discord | Flatpak or third-party | Optional, portable IPC |
| Bottles / Wine / Proton | Flatpak or RPM Fusion | Detect only; do not install or manage |
| Noctalia shell | Native Wayland shell with IPC (`noctalia-kwe msg`) and GTK theme templates | Consume notifications/DND and the generated GTK theme; never load its plugins |

The port must stop assuming Arch/AUR paths. Discovery must use, in order: `PATH`, the XDG
data directories, Flatpak exports under `~/.local/share/flatpak/exports` and
`/var/lib/flatpak/exports`, and documented per-application config locations. A source that
is not installed is hidden or shown as unavailable, never guessed from an Arch-only path.

### 2.4 KineticWE and Noctalia platform surface

The design target is the KineticWE session:

| Facility | What KineticWE/Noctalia provides | Use in the port |
| --- | --- | --- |
| Compositor | KWin fork, Wayland, `org.kde.KWin` + `org.kde.kwin.Scripting` D-Bus, `kde-output-management-v2`, `org_kde_plasma_virtual_desktop` | Game Mode window/workspace/output control |
| Layer shell | Not provided to clients by the target KineticWE build; the source tree contains an implementation, but it is not a supported client surface | No guide overlay; no `gtk4-layer-shell` dependency |
| Global shortcuts | Embedded `KGlobalAccelD` owning `org.kde.kglobalaccel`; portal `GlobalShortcuts` | Game Mode shortcut without editing Lua files |
| Screen capture | `org.kde.KWin.ScreenCast` / Screenshot and `xdg-desktop-portal-kwe` (PipeWire) | Optional highlights/guide stills later |
| Idle inhibit | compositor + `idleinhibit` protocol | Keep the screen awake only while a game runs |
| Notifications | Noctalia notification daemon; IPC `notification-dnd-*` | Silence/restore DND in Game Mode |
| Theme | Noctalia palette/templates write GTK 3/4 theme and color scheme | App follows GTK named colours; ThemeWatcher reloads |
| Shell IPC | `noctalia-kwe msg <command>` over a Unix socket | DND, optional shell actions, diagnostics |
| Config | Noctalia config is its own; no app writes into it | Read-only integration, no plugin loading |

This is a much better fit than the upstream Omarchy assumptions: the protocols and services
exist, they are owned by the target desktop, and they are documented in the KineticWE/Noctalia
trees. The port should use them directly rather than emulate them.

---

## 3. Toolkit mapping

This is the core of the feasibility answer. Every Qt facility has a gtkmm/GLib equivalent,
but the types and idioms are different. See [03-Architecture](03-ARCHITECTURE.md) for the
component mapping.

| Qt facility | GTK port approach | Risk |
| --- | --- | --- |
| `QString`, `QByteArray` | `std::string`, `std::string_view`, `Glib::ustring` only at GTK boundaries | Low; mechanical but wide |
| `QVariantMap` / `QVariantList` | Typed structs plus `Glib::VariantBase`/JSON-GLib at boundaries | Medium; removes implicit conversions |
| `QObject` signals/slots | `sigc::signal`, `Glib::Property`, plain callbacks | Low |
| `QAbstractListModel` | `Gio::ListModel` + `Glib::ListStore`, `Gtk::FilterListModel`, `Gtk::SortListModel` | Medium; model roles become typed objects |
| QML / Qt Quick | gtkmm widgets, `Gtk::ListView`/`Gtk::GridView`, custom widgets, CSS | High effort; the largest single work item |
| Qt Quick Controls | gtkmm stock controls plus LGL component classes | Low–medium |
| `QSqlDatabase` (SQLite) | `sqlite3` prepared statements with an RAII wrapper | Low; schema can be reused |
| `QNetworkAccessManager` | libsoup 3 with `Gio::MainLoop` integration | Medium; async model differs |
| `QProcess` | `Gio::Subprocess` with argv only | Low |
| `QLocalServer` / `QLocalSocket` | `Gio::SocketService` / `Gio::SocketConnection` | Low |
| `QDBus` | `Gio::DBus` | Low; little current use |
| `QFileSystemWatcher` | `Gio::FileMonitor` | Low |
| `QSettings` + custom TOML | `Glib::KeyFile` in `settings.ini`, with a TOML importer | Low |
| `QImage`, `QPixmap`, `QPainter` | `GdkPixbuf`, `Gdk::Texture`, Cairo for generated cards | Medium |
| Qt SVG | GdkPixbuf SVG loader / librsvg | Low |
| `QRegularExpression` | `Glib::Regex` (PCRE2) | Low |
| `QJsonDocument` | JSON-GLib | Low |
| `QDateTime` | `std::chrono` + `Glib::DateTime` | Low |
| `QStandardPaths` | `Glib::get_user_config_dir`, `get_user_data_dir`, `get_user_cache_dir`, `Glib::get_user_special_dir` | Low |
| `QLockFile` | `flock`/`Gio::File` advisory lock | Low |
| Qt Concurrent / `QFutureWatcher` | `std::jthread`, `GThreadPool` or `Gio::Task`, results through `Glib::Dispatcher` | Medium |
| Qt Test | GLib test framework (`g_test_*`) through CTest | Medium; all tests are rewritten |
| LayerShellQt | No GTK equivalent on the target because the protocol is absent | Not available |
| Qt Wayland `GuiPrivate` idle inhibit | `Gtk::Application::inhibit` / `Gtk::Window` idle inhibition, or a portal | Low |
| QML singleton `UiMetrics` | a C++ metrics object plus CSS variables | Low |

The important conclusion: **there is no missing library**. The port is a large translation
and redesign effort, not a research problem.

---

## 4. Feature feasibility

Detailed disposition is in [06-Feature-Parity](06-FEATURE-PARITY.md). Summary:

| Feature group | Feasibility | Notes |
| --- | --- | --- |
| Launcher discovery (Steam, Heroic, Lutris, GOG, RetroArch, emulators, ROM folders, manual) | **Direct port** | Parsers and fixtures can be translated; paths need Fedora discovery |
| Unified library, filters, search, sort, collections, tags, saved filters | **Direct port** | Data model moves to `Gio::ListModel`; no semantic change |
| Game details, artwork, metadata, IGDB, ProtonDB, SteamGridDB | **Direct port** | Network client replaced by libsoup 3; artwork pipeline rebuilt on GdkPixbuf |
| Launch delegation, emulator argv, Flatpak launch, launch setups | **Direct port** | `Gio::Subprocess`, argv-only policy preserved |
| Play-session recording and statistics | **Direct port** | Keep SQLite schema and separate `sessiond` process |
| Library backup and restore | **Direct port** | Keep `.omakade-backup` v1/v2 compatibility; use libzip |
| Emulator save protection | **Direct port** | Largest safety-critical core module; keep journals and bounds |
| Achievements (Steam, RetroAchievements, Xbox) | **Port with work** | API clients replace Qt Network; Xbox depends on external Xodus data |
| Native ports, Kyty, RomM, Battle.net, Xodus | **Port with work** | Detection and parsers are portable; some are optional |
| Sunshine publishing | **Port with work** | External JSON file; no compositor dependency except output selection |
| Controller navigation and Game Mode | **Redesign** | SDL 3 stays; focus engine and fullscreen layout written for GTK |
| Game Mode display/audio/hold/notifications | **Feasible, verify** | KWin D-Bus/scripting, output management, Noctalia DND IPC; each capability probed; other compositors degrade |
| In-game guide over a game | **Not available** | KineticWE does not provide `zwlr_layer_shell_v1`; no GTK overlay is possible without compositor support |
| Highlight replay buffer | **Conditional/later** | No Fedora-packaged `gpu-screen-recorder`; KineticWE ScreenCast portal makes a future PipeWire design possible |
| Omarchy theme | **Drop** | Replaced by Noctalia-generated GTK theme + libadwaita named colours + ThemeWatcher |
| Omarchy shell plugin | **Drop** | No replacement until KineticWE provides a supported overlay surface |
| Hyprland Lua key-binding injection | **Drop** | Replace with KGlobalAccel / GlobalShortcuts portal and Kinetic Settings documentation |
| Omarchy do-not-disturb | **Replace** | Noctalia `notification-dnd-set` / `notification-dnd-status` IPC |

---

## 5. The real blockers

### 5.1 Omarchy theming is not portable

`src/theme/OmarchyTheme.cpp` reads `~/.local/state/omarchy/current/theme/colors.toml`,
`shell.toml`, Hyprland metrics and the user's monospace font, and exposes 20+ colours to QML.
Fedora has no Omarchy. The GTK standard already defines the replacement: named libadwaita
colours, a prefixed derived palette, a ThemeWatcher that reloads user CSS live, and fixed
status colours. This is a deletion and replacement, not a port.

**Impact:** low. It removes code and aligns the app with every other LGL GTK app.

### 5.2 Hyprland-specific Game Mode must become KineticWE-specific

`GameModeDesktop.cpp` shells out to `hyprctl` and uses Hyprland Lua evaluation to:

- list and enable/disable outputs;
- find windows by PID and workspace;
- move windows between workspaces and outputs;
- hold launcher windows out of sight;
- restore focus, workspace, fullscreen state and the desktop;
- identify a placeholder window.

KineticWE is a KWin fork, so the primitives exist but the API is different:

- `org.kde.KWin` and `org.kde.kwin.Scripting` provide workspace, window and script control.
- `kde-output-management-v2` / `org.kde.KWin` output D-Bus provide output enumeration and
  enable/disable.
- `org.kde.KWin.VirtualDesktopManager` provides virtual desktops.
- KWin scripting can move windows, hold them and restore geometry; it is powerful and must be
  treated as a security-sensitive, read-only bundled script, not user-authored code.
- The embedded `KGlobalAccelD` provides global shortcuts through `org.kde.kglobalaccel`.

**Impact:** medium, not a blocker. The interface is a new `KineticWECompositor` adapter plus a
small bundled KWin script. The script is shipped in the GResource, loaded by absolute path,
never generated from untrusted data, and removed when the session ends. Other compositors get
`UnavailableCompositor` and lose only the compositor-dependent parts.

### 5.3 The in-game guide is an Omarchy shell plugin

`omarchy-plugin/` is a Quickshell/QML plugin for the Omarchy shell, with a guide protocol,
focus handling, live capture and audio widgets. It cannot be loaded into Noctalia (a native
Wayland shell with no GTK or QML) or into the GTK application.

**Impact:** medium, and not solvable inside this application. KineticWE does not provide
`zwlr_layer_shell_v1`, so a GTK 4 layer-shell overlay cannot be created over a fullscreen game.
The guide is dropped from the target feature set. A future version would require the compositor
to expose a supported overlay surface; until then the UI says the guide is unavailable. The
guide data protocol can still be designed for that future, but no overlay code ships.

Do not substitute a normal fullscreen window and call it an overlay: it can steal focus from
the game and is not equivalent.

### 5.4 Highlights assume `gpu-screen-recorder`

The replay buffer starts `gpu-screen-recorder` as a child with an IPC socket. Fedora does
not ship that program in its official repositories. KineticWE exposes `org.kde.KWin.ScreenCast`
and the `xdg-desktop-portal-kwe` ScreenCast backend, so a future recorder can use the
ScreenCast portal and PipeWire.

**Impact:** medium. Highlights are P3. The rest of the library does not depend on them, and a
future recorder requires its own security review (consent, indicator, bounded buffer, audio
opt-in).

### 5.5 KineticWE does not provide layer-shell

The target KineticWE build does not expose `zwlr_layer_shell_v1` to clients. The source tree
contains an implementation, but it is not part of the supported surface the application can
rely on. Therefore:

- `gtk4-layer-shell` is not a dependency and is not used.
- The in-game guide overlay is not available on the target.
- A normal fullscreen guide window is not a substitute: it can steal input from the game.
- The UI must report the guide as unavailable rather than silently doing nothing.

A future guide requires the compositor to provide a supported overlay mechanism. That is an
upstream KineticWE change, not something this application can work around safely.

### 5.6 Scale of the QML rewrite

The QML is not a thin skin. `Main.qml` alone is 4,201 lines, the settings panel 2,159,
game details 2,374, and the library/console views several thousand more. GTK widgets behave
differently from Qt Quick: no declarative bindings, different focus and sizing, no anchors.
This is the largest single cost and the main schedule risk.

**Impact:** high but tractable if the UI is rebuilt page by page with the standard's
components instead of translated line by line.

---

## 6. Effort estimate

Planning ranges for one experienced C++/GTK developer with AI assistance, including tests
and documentation. They assume the P0 scope is fixed and no new features are invented.

| Phase | Scope | Range |
| --- | --- | --- |
| 0 | Design, security review, decisions (this package) | 1–2 weeks |
| 1 | Foundation: build, shell, settings, theme, database wrapper, CI, snapshot harness | 3–4 weeks |
| 2 | Library read-only: P0 sources, unified model, grid/list, search, details, artwork | 6–8 weeks |
| 3 | Launch and tracking: launch planner, sessiond, statistics, single instance, CLI | 5–7 weeks |
| 4 | Organisation and metadata: tags, collections, filters, editors, IGDB/ProtonDB, achievements | 8–10 weeks |
| 5 | Backup, saves, remaining sources, Sunshine, RomM | 8–10 weeks |
| 6 | Controller and Game Mode, KineticWE/Noctalia adapters | 7–10 weeks |
| 7 | Optional highlights only (guide blocked by missing layer-shell) | 4–6 weeks |
| 8 | Hardening, accessibility, fuzzing, packaging, release | 4–6 weeks |

Totals:

- **P0 usable release:** about 15–21 developer-weeks.
- **P1 complete (through Phase 6):** about 38–51 developer-weeks.
- **Full P2 parity (through Phase 8):** about 46–63 developer-weeks.

These ranges are large because the upstream is large. The plan reduces risk by making each
phase independently useful and by deferring the recorder-dependent features.

---

## 7. Conditions for success

The port is feasible **if** all of the following hold:

1. The P0 scope is accepted: desktop library, launch, tracking and a small source set.
2. KineticWE/Noctalia is treated as the target, with other Wayland sessions as compatibility,
   not as equal design inputs.
3. The GTK standard's application identity, theme, title bar, menu and settings rules are
   followed from the first commit, not retrofitted.
4. Core logic stays widget-free and is tested without a display.
5. The SQLite schema and backup format are treated as compatibility contracts.
6. Every external input is bounded and validated; no feature is allowed to bypass
   [09-Security](09-SECURITY.md).
7. CI builds and tests in a Fedora container, not on the host.
8. The upstream Qt tree is archived, not kept as a second active implementation.
9. The KWin script and any Noctalia IPC used by the app are shipped, read-only, and validated;
   no plugin code from Noctalia is executed.
10. No in-game overlay is promised while KineticWE lacks a supported overlay surface.

## 8. What would make it infeasible

- A requirement for pixel-identical Omarchy theming or the Omarchy shell plugin.
- A requirement for an in-game overlay on KineticWE without compositor support.
- A requirement to support every launcher and emulator at 1.0 with no staged rollout.
- A refusal to keep the security limits (no shell, bounded archives, Secret Service).
- Reliance on an undocumented KWin scripting side effect that changes every release without
  a capability probe.
- Loading untrusted Noctalia plugins or arbitrary KWin scripts from the library database.

None of these is part of the recommended scope.

## 9. Verdict

**Proceed with a staged GTK port for KineticWE + Noctalia.** The core application is a large
but straightforward translation to gtkmm 4. The desktop-specific value of Omakade maps well
to the target: KWin provides the window, workspace, output, shortcut and screen-cast surface;
Noctalia provides notifications, DND and GTK theme generation. The in-game guide is **not
available** because KineticWE does not provide layer-shell; it is an upstream prerequisite, not
something the app can fake. The recorder remains optional. That is a better product than a Qt
application with a GTK skin or an Omarchy plugin that cannot run on the target desktop.
