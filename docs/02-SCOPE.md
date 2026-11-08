# 02 — Product scope

This document defines what LGL Games Library is, who it is for, what ships in each release
tier, and what is explicitly out of scope. It is the authority for “is this in the port?”
questions.

Status: draft for approval, 2026-11-08.

---

## 1. Product statement

LGL Games Library is a local-first game library for Fedora. It collects installed games
from the launchers and emulators the user already uses, presents one consistent library,
and starts each game through the platform that owns it. It never installs, updates, cracks
or manages games itself. Accounts, downloads, DRM, cloud saves and compatibility settings
stay with the owning launcher.

It is a native GTK 4 (gtkmm) application that looks and behaves like every other
LinuxGamerLife GTK app.

### One-sentence test

> “Show me every game I can play, let me find one, start it, and tell me how long I played.”

If a feature does not serve that sentence or the organisation around it, it is not P0.

---

## 2. Users

| User | Needs | Design consequence |
| --- | --- | --- |
| Fedora desktop player | One place to see Steam, Heroic, Lutris, GOG and emulator games | Source discovery must work with RPM and Flatpak installs |
| Couch/TV player | Controller navigation, readable cards, fullscreen mode | Controller focus engine and Game Mode are first-class, not an afterthought |
| Emulator user | Per-console cards, ROM folders, save protection | Console catalogue and save layouts are core, not plugins |
| Library organiser | Favourites, tags, collections, saved filters, custom art | All organisation is local and exportable |
| Privacy-conscious user | No account, no telemetry, no forced network | Offline first; network features opt-in and documented |
| Packager/maintainer | Reproducible Fedora build, no root, clear dependencies | RPM/COPR first, standard build commands |

Non-user: the application is not a store, a compatibility layer, a Wine manager, a ROM
site, a scraper that uploads files, or a remote control service.

---

## 3. Supported environment

| Item | Requirement |
| --- | --- |
| Operating system | Fedora 44 or later |
| Session | Wayland |
| Desktop | **KineticWE (KWin-based compositor) + Noctalia shell** — the primary and design target |
| Other sessions | Stock KDE Plasma, GNOME and wlroots are best-effort compatibility, not targets |
| Toolkit | GTK 4 + gtkmm 4.20+, libadwaita 1.6+ for initialisation and style manager only |
| Language | C++20 |
| Build | CMake 3.20+, Ninja, `glib-compile-resources`, GCC and Clang |
| Packaging | RPM built on COPR; Flatpak explicitly later |
| Architecture | x86_64 first; aarch64 builds in CI but hardware acceptance is separate |

Fedora 43 is not targeted. Supporting two Fedora releases is a release-engineering decision
made after 1.0.

---

## 4. Release tiers

### P0 — 0.1 “Library”

The smallest release that is genuinely useful.

- GTK shell: header bar, logo, main menu, both title bar modes, standard shortcuts, About,
  notices, settings window, help.
- Discover installed games from:
  - Steam (native and Flatpak)
  - Heroic (Flatpak and native)
  - Lutris (native and Flatpak)
  - direct GOG installations
  - RetroArch playlists
  - ROM folders
  - manual entries
- Unified library with search, source filters, sort and a responsive cover grid.
- Game details with local artwork, installation path, source and a Play button.
- Launch through the owning launcher, emulator or direct command; Flatpak launch support.
- Single instance and command-line activation.
- Play-session tracking through the separate `lgl-games-library-sessiond` service, with
  per-game totals and a basic statistics page.
- Settings persisted in `settings.ini`; import from Omakade `config.toml`.
- Import an existing Omakade library database and artwork cache.
- Offline operation with no account.

P0 does **not** include: Game Mode, controller navigation, achievements, network metadata,
save protection, backup/restore, highlights, guide, Sunshine, RomM, Battle.net/Xodus, Year in
Review.

### P1 — 0.5 “Organise”

- Favourites, hidden games, completion status, tags, collections, saved filters, bulk
  organisation.
- Custom artwork editor (cover, hero, logo, icon) and artwork cache management.
- Game links and installation selection when several sources hold the same game.
- Remaining local sources: PCSX2, RPCS3, PPSSPP, Dolphin, Cemu, melonDS, Ryujinx/Eden,
  shadPS4, Xenia, Kyty, native ports, Battle.net, console portals.
- Metadata: IGDB (critic score, play-time estimates), ProtonDB badges, SteamGridDB portraits.
- Achievements: Steam, RetroAchievements, Xbox via Xodus data where available.
- Library backup and restore with the `.omakade-backup` v1/v2 format.
- Emulator save protection with snapshots, restore journal and coverage UI.
- Statistics, play history, Year in Review card export.
- Basic controller navigation and fullscreen Game Mode on the current display.
- Discord Rich Presence (opt-in).
- Sunshine/Moonlight export.
- RomM optional library.

### P2 — 1.0 “Console”

- Dedicated Game Mode display and sound output through KineticWE's output and KWin control.
- Launch hold and desktop restoration through the KWin scripting adapter.
- Notifications silenced and restored through Noctalia's `notification-dnd-*` IPC.
- Controller Home button and capture button.
- No in-game guide overlay: KineticWE does not provide layer-shell, so the upstream guide has
  no overlay implementation. The UI reports this; no fake overlay ships.
- Highlight replay buffer with an external recorder or a new ScreenCast/PipeWire design.
- Steam achievement clips.
- Remaining polish, accessibility and performance work.

P2 features are individually gated. Another compositor that cannot support one does not block
the release of the others; the UI states why a feature is unavailable.

### P3 — optional adapters

- XDG Global Shortcuts portal binding for Game Mode (KineticWE provides it).
- Flatpak packaging with a documented sandbox profile.
- KWin scripting improvements for unusual window layouts.
- wlroots foreign-toplevel adapter for other Wayland sessions.
- Additional launchers that appear on Fedora.

### Dropped

These upstream features are not ported:

| Upstream feature | Reason |
| --- | --- |
| Omarchy theme reading (`OmarchyTheme`) | Replaced by Noctalia-generated GTK theme + ThemeWatcher |
| Omarchy shell in-game guide plugin | KineticWE does not provide layer-shell; no overlay is possible |
| In-game guide overlay | Same; requires a future compositor overlay surface |
| Hyprland Lua binding injection | Replace with KGlobalAccel / GlobalShortcuts and Kinetic Settings |
| Omarchy do-not-disturb through `omarchy-shell` | Replaced by Noctalia `notification-dnd-*` IPC |
| `pactl`-only assumptions | Fedora uses PipeWire; keep an adapter with `pactl` compatibility |
| AUR/Omarchy package assumptions | Fedora packaging and discovery rules replace them |

---

## 5. Source support matrix

| Source | P0 | P1 | Notes |
| --- | --- | --- | --- |
| Steam | yes | | Native and Flatpak; `libraryfolders.vdf`, app manifests, local artwork |
| Heroic | yes | | Epic/GOG/Amazon caches; installed and owned views |
| Lutris | yes | | Native and Flatpak; YAML database |
| GOG direct | yes | | `~/GOG Games`, `~/Games/GOG`, `~/Games/Heroic`, extra folders, `umu-run` |
| RetroArch | yes | | Playlists, cores, local thumbnails |
| ROM folders | yes | | Console catalogue, EmuDeck-style layouts |
| Manual | yes | | User-entered command, validated and shown |
| PCSX2 | | yes | Native/Flatpak roots, memory cards |
| RPCS3 | | yes | `PARAM.SFO`, installed games, saves |
| PPSSPP | | yes | `PARAM.SFO` inside ISO/PBP, `SAVEDATA` |
| Dolphin | | yes | Game folders, cache covers, save banks |
| Cemu | | yes | Title IDs, MLC roots, save folders |
| melonDS | | yes | ROM header, battery saves |
| Ryujinx/Eden | | yes | NSP/XCI metadata, prod.keys in memory only |
| shadPS4 | | yes | Title saves, profiles |
| Xenia | | yes | Recent games, `default.xex`, ISO/XEX/ZAR |
| Kyty | | yes | `param.json`, launcher wrappers |
| Native ports | | yes | AppImages/desktop entries + bundled catalogue |
| Battle.net | | yes | Agent `product.db` in Wine/Proton/Bottles prefix |
| Xodus Nexus | | yes | Microsoft Store/PC Game Pass shortcut data |
| RomM | | yes | Optional server, user-supplied URL and credentials |
| Faugus | | yes | Detect when present; no Fedora package promise |

A source that is not installed is not an error. It is hidden from the source list, or shown
as “not detected” in settings with a short explanation.

---

## 6. Machines and data

| Data | Location | Notes |
| --- | --- | --- |
| Settings | `$XDG_CONFIG_HOME/lgl-games-library/settings.ini` | `Glib::KeyFile`; one typed getter/setter per key |
| Library database | `$XDG_DATA_HOME/lgl-games-library/library.sqlite3` | SQLite, WAL, shared with sessiond |
| Artwork overrides | `$XDG_DATA_HOME/lgl-games-library/artwork/` | User-selected images, content-addressed |
| Save sets | `$XDG_DATA_HOME/lgl-games-library/save-sets/` | Bounded, journalled |
| Backups | user-selected path | `.omakade-backup` compatible |
| Cover/artwork cache | `$XDG_CACHE_HOME/lgl-games-library/` | Bounded by a setting |
| Runtime socket | `$XDG_RUNTIME_DIR/lgl-games-library/` | Mode 0700 directory; 0600 socket |
| Clips (P2) | `$XDG_VIDEOS_DIR/LGL Games Library/<game>/` | Only if highlights are enabled |
| Logs | stderr/journal | No secrets, no personal paths beyond what is needed |

Existing Omakade data is imported, never written in place. See
[04-Data model](04-DATA-MODEL.md) and [15-Migration](15-MIGRATION.md).

---

## 7. Non-goals

- Installing, updating, downloading or patching games, ROMs, emulators or launchers.
- Managing Steam/Epic/GOG accounts or DRM.
- Uploading ROMs, saves, screenshots or personal data to any service.
- Cloud sync of the library.
- A web server, remote control or LAN API beyond the optional Sunshine app export.
- A plugin system in 1.0.
- Running as root or providing a privileged helper.
- Windows or macOS support.
- Pixel-identical reproduction of Omakade’s QML layout.
- A store, review site, social network or telemetry service.
- Replacing RetroArch, Steam Input, gamescope or the desktop’s compositor.

---

## 8. Success criteria

### P0

- A new Fedora user with Steam and RetroArch installed sees their games without editing a
  config file.
- Search returns a game in under 200 ms with 1,000 games.
- Play starts the correct launcher/emulator and records a session that survives a restart.
- The app closes and reopens without losing settings or library state.
- A cancelled file dialog, missing launcher, unreadable manifest or killed game process
  never crashes the app or blocks the main loop.
- The GTK standard checklist in [18-Standards compliance](18-STANDARDS-COMPLIANCE.md) is
  fully met.

### P1

- Organisation survives export and restore.
- A save restore interrupted at any point either completes or is recoverable without
  overwriting unexpected external changes.
- Metadata and achievement caches work offline after one successful fetch.

### P2

- Game Mode can be entered and left on KineticWE, restoring the window, workspace, output and
  audio state.
- Noctalia notifications are silenced and restored, or the UI says the IPC is unavailable.
- The in-game guide is reported as unavailable on KineticWE; no overlay is shipped.
- Another compositor that cannot support a feature reports that clearly.

### Project-level

- No High or Critical security finding is unresolved at release.
- CI builds and tests on Fedora 44 with GCC and Clang.
- The source archive contains only the GTK implementation; `legacy/qt/` is excluded.
- No Qt package is a build, test, runtime or packaging dependency.
- A KineticWE session with Noctalia is the reference acceptance environment.

---

## 9. Assumptions

1. The user runs Fedora with KineticWE and Noctalia; other sessions are compatibility.
2. KineticWE keeps `org.kde.KWin`, KWin scripting, KGlobalAccel and the output-management
   protocols available. It does not provide layer-shell; the app does not depend on it.
3. Noctalia keeps the `notification-dnd-*` IPC and GTK theme templates.
4. Launchers keep their data in documented or stable-enough local files; changes are
   handled as parser updates with fixtures.
5. Flatpak installs are discoverable through exported desktop files and app IDs.
6. The upstream GPL licence and artwork attribution are preserved.
7. The port is maintained independently; upstream Qt changes are reviewed, not merged
   blindly.

## 10. Open questions

These require a decision before the phase that depends on them. Track them in
[17-Risks and decisions](17-RISKS-DECISIONS.md).

| ID | Question | Needed by |
| --- | --- | --- |
| Q1 | Which KineticWE API is stable enough to depend on: KWin D-Bus methods, a bundled KWin script, or both? | P0 architecture |
| Q2 | Is a source-compatible fork acceptable, or must imports be one-way only? | P0 data model |
| Q3 | Should the app id be `com.linuxgamerlife.lgl-games-library` or a shorter existing brand? | P0 identity |
| Q4 | Is Game Mode a 1.0 requirement or a post-1.0 feature? | P1 planning |
| Q5 | Are achievements required for 1.0, or can they be a later optional connection? | P1 planning |
| Q6 | Is Flatpak support a goal at all, given host launcher access? | Packaging |
| Q7 | Which external recorder (if any) is acceptable for highlights, or should the app use the KineticWE ScreenCast portal? | P2/P3 |
| Q8 | Does the project inherit Omakade’s GPL notices verbatim or add a separate copyright line? | Legal |
| Q9 | Should the app provide a guide-button helper for Home/Capture actions that do not need an overlay? | Phase 7 |
