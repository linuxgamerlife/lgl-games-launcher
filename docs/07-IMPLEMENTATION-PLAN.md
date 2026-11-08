# 07 — Implementation plan

This document turns the architecture into an ordered work plan. It is written for one
experienced C++/GTK developer working with AI assistance, with review from the project
maintainer. Estimates are planning ranges and are re-based at the end of Phase 1.

Status: proposed. No implementation has started.

---

## 1. Working rules

1. **Plan before code.** Every phase starts with a short design note and a security review of
   the features in that phase.
2. **One behaviour per pull request.** A parser, a model, a page or an adapter, with tests.
3. **Core first.** A feature gets its core implementation and tests before its GTK page.
4. **No build on the host.** Configure and test in the Fedora container described in
   [11-Build](11-BUILD.md). The upstream KineticWE rule that forbids agents from building is
   not inherited; this repository's CI and container are the build environment.
5. **No Qt.** The Qt tree is archived in `legacy/qt/` and is never in a build target.
6. **Security gates.** A phase cannot exit with an open High/Critical finding or an
   unmitigated threat in its design review.
7. **Data compatibility.** Schema and backup changes include a migration and a real fixture.
8. **Definition of done** is in [19-Acceptance criteria](19-ACCEPTANCE-CRITERIA.md).

---

## 2. Phase overview

| Phase | Theme | Duration | Release |
| --- | --- | --- | --- |
| 0 | Design and approvals | 1–2 weeks | — |
| 1 | Foundation and shell | 3–4 weeks | internal |
| 2 | Library read-only | 6–8 weeks | 0.1 |
| 3 | Launch and tracking | 5–7 weeks | 0.2 |
| 4 | Organisation and metadata | 8–10 weeks | 0.5 |
| 5 | Backup, saves, remaining sources | 8–10 weeks | 0.7 |
| 6 | Controller and Game Mode | 7–10 weeks | 0.9 |
| 7 | Optional recorder; guide remains blocked | 4–6 weeks | 1.0 |
| 8 | Hardening and release | 4–6 weeks | 1.0 |

P0 is Phases 1–2 plus the launch part of Phase 3. “0.1” means useful, not feature-complete.

---

## 3. Phase 0 — Design approval

### Tasks

- Review [01-Feasibility](01-FEASIBILITY.md), [02-Scope](02-SCOPE.md),
  [03-Architecture](03-ARCHITECTURE.md) and [09-Security](09-SECURITY.md).
- Resolve open decisions Q1–Q9 in [17-Risks and decisions](17-RISKS-DECISIONS.md).
- Confirm the application identity and licence notices.
- Confirm the KineticWE version/commit used as the reference platform.
- Confirm the Noctalia IPC command set and DND semantics from the target build.
- Freeze the P0 source list and the P0 screen list.

### Exit criteria

- P0 scope signed off.
- Security review signed off.
- No unresolved High risk without an owner and mitigation.
- The KineticWE/Noctalia capability assumptions are recorded with versions.

---

## 4. Phase 1 — Foundation and shell

### Goal

A buildable GTK application that opens a standard LGL window, stores settings, follows the
theme, shows an empty library, and passes its first tests. No sources yet.

### Tasks

1. **Repository and build**
   - Root `CMakeLists.txt`, `Makefile`, `.copr/Makefile`.
   - `data/` GResource: `style.css`, app icon, empty-state strings.
   - `LGL_GAMES_LAUNCHER_VERSION`, sanitizer option, warnings as errors.
   - `.clang-format`, `.clang-tidy`, `.gitignore`, CI workflow.
2. **Application shell**
   - `main.cpp`, `Gtk::Application`, `gameslauncher::Window`.
   - Header bar, logo, wordmark, main menu, About, Help placeholder.
   - Both title bar modes and Ctrl+Shift+M.
   - Alt menu behaviour, standard shortcuts, notices overlay.
   - Empty library page and an empty details placeholder.
3. **Theme**
   - `src/core/theme/` pure helpers and tests.
   - `src/ui/themewatcher/` live reload, debounce, no-portal fallback, mono font, `.dark`.
   - `style.css` with named colours and `gl_` derived palette.
4. **Settings**
   - `Settings` class with `Glib::KeyFile`, typed getters/setters, test path.
   - `settings.ini` created 0600; folder 0700.
   - TOML importer scaffold (can be completed in Phase 2).
5. **Paths and database**
   - XDG path resolver and tests.
   - `sqlite3` RAII wrapper, connection settings, migration runner.
   - Open an empty library database and create the schema skeleton.
6. **Platform basics**
   - `ProcessRunner` over `Gio::Subprocess`.
   - Logging domain.
   - `SecretStore` detection (no secrets yet).
   - Capability probe for KineticWE/Noctalia and a Diagnostics page.
7. **Test and snapshot harness**
   - `settings_test`, `theme_test`, `paths_test`, `db_test`.
   - `LGL_GAMES_LAUNCHER_GTK_SNAPSHOT` smoke test.
   - CI builds Debug and Release with GCC and Clang in Fedora 44.

### Exit criteria

- `cmake -S . -B build && cmake --build build` succeeds on Fedora 44.
- `ctest` passes, with display tests skipping cleanly when no display is available.
- The GTK standard checklist items for shell, theme, title bar, menu, About, settings and
  snapshot are met.
- No Qt include or package appears in the build.
- The snapshot folder contains a rendered empty library and settings window.

### Risks

- gtkmm boilerplate for custom widgets is slower than expected. Mitigation: build reusable
  `Card`, `SectionLabel`, `SettingsRow`, `Notice` components early.
- ThemeWatcher behaviour differs from the existing LGL reference apps. Mitigation: copy the
  proven implementation and its tests, then adapt.

---

## 5. Phase 2 — Library read-only (0.1)

### Goal

Discover installed games from the P0 sources, show them in a responsive GTK grid, search and
filter them, and open a details page. Launch is Phase 3.

### Tasks

1. **Domain model**
   - `SourceId`, `GameKey`, `Installation`, `Game`, `Artwork`.
   - Normalisation, validation, stable identity.
2. **Source framework**
   - `Source` interface, registry, detection, scan scheduling and cancellation.
   - Transactional upsert into source cache tables.
   - Error aggregation and diagnostics.
3. **P0 scanners**
   - Steam (`libraryfolders.vdf`, `appmanifest_*.acf`).
   - Heroic (Epic/GOG/Amazon caches).
   - Lutris YAML.
   - Direct GOG with `umu-run`.
   - RetroArch playlists and cores.
   - ROM folders and the 45-system console catalogue.
   - Manual games.
4. **Unified library**
   - Merge installations, prefer one, group by console.
   - Filters: source, installed, console, search.
   - Sort modes that do not need metadata (title, recent, playtime, installed).
   - Saved filter placeholder (full in Phase 4).
5. **GTK library UI**
   - `Gtk::GridView` with `Glib::ListStore` of `GameObject`.
   - Card factory, placeholder, selection, focus, hover.
   - Sidebar and filter bar.
   - Empty state and scan progress.
   - Search entry and Ctrl+F.
6. **Details (basic)**
   - Cover, title, source, platform, path, Play placeholder.
   - Back navigation.
7. **Artwork**
   - Local launcher art, console art, cache, bounded decode.
   - Custom artwork not yet.
8. **Settings import**
   - Complete `config.toml` → `settings.ini` importer.
   - Import existing Omakade database and artwork cache into the new paths (copy, verify,
     never modify the original).

### Exit criteria

- A fixture library of at least 200 games across all P0 sources displays correctly.
- Search returns in under 200 ms with 1,000 rows.
- A scan never blocks the UI for more than 50 ms.
- Malformed launcher files produce a source warning, not a crash.
- The app starts, scans and exits cleanly with no leaks under ASan.
- Snapshot tests cover empty, populated, list, details and console pages.

---

## 6. Phase 3 — Launch and tracking (0.2)

### Goal

Play a game through its owning launcher or emulator and record the session.

### Tasks

1. **Launch planner**
   - `LaunchPlan` builder per source.
   - Allow-listed executables, path validation, environment allowlist.
   - Flatpak argv.
   - Manual command parser without a shell.
2. **Launch executor**
   - `Gio::Subprocess`, detached, bounded error capture.
   - Launch failure notices.
   - Single-instance commands `play`, `activate`, `quit`.
   - `--play`, `--quit`, `--demo` CLI.
3. **Session daemon**
   - Process polling, profiles, matching.
   - Open/extend/close sessions, monotonic time, flush.
   - Ownership lock, durable journal, replay.
   - systemd user unit.
4. **Statistics (basic)**
   - Totals per game, recent sessions, source breakdown.
   - Delete history.
5. **Database**
   - Keep upstream schema compatibility; run the schema diff and migration tests.
   - Test with a sanitized Omakade-created database.

### Exit criteria

- Steam, Heroic, Lutris, GOG, RetroArch, a ROM-folder game and a manual game launch correctly
  from a fixture environment.
- A launched process is recorded once, with correct start/end and no duplicate rows after a
  daemon restart.
- A killed daemon loses at most one flush interval and replays refused writes.
- No shell invocation is reachable from any launch path (grep and unit tests).
- The app survives a missing launcher, a missing emulator, a bad path and a killed game.

---

## 7. Phase 4 — Organisation and metadata (0.5)

### Goal

Make the library personal and informative.

### Tasks

1. Organisation: favourites, hidden, completion, tags, collections, saved filters, bulk
   organise, undo.
2. Links and preferred installations; installation picker.
3. Metadata editor; regional titles.
4. Artwork editor: cover/hero/logo/icon, crop/preview, reset.
5. Network clients: libsoup 3, TLS, timeouts, size caps, host allowlists.
6. IGDB, ProtonDB, SteamGridDB; caches and connection UI.
7. Steam achievements, RetroAchievements; achievement views.
8. Stats page full: charts, periods, top games, Year in Review export.
9. Settings pages: Sources, Library, Connections, Storage, Privacy, About.
10. Diagnostics: source status, capabilities, database path, log copy.

### Exit criteria

- Organisation survives a restart and an export/import round trip.
- A malformed or hostile network response is rejected and the previous cache remains.
- No secret is written to a non-secret store; a test scans logs and files for fixture secrets.
- IGDB/Steam/RA caches work offline after one fetch.
- The Settings snapshot is complete and all controls are keyboard reachable.

---

## 8. Phase 5 — Backup, saves, remaining sources (0.7)

### Goal

Protect what the user has and support the long tail of sources.

### Tasks

1. Backup archive read/write, preview, merge/replace, recovery, ASCII/format compatibility.
2. Save layouts and adapters for RetroArch, Dolphin, PCSX2, Cemu, shadPS4, Ryujinx, melonDS and
   native fallbacks.
3. Save snapshot, verify, restore, journal, recovery, coverage UI, cleanup.
4. Remaining emulators: PCSX2, RPCS3, PPSSPP, Dolphin, Cemu, melonDS, Ryujinx/Eden, shadPS4,
   Xenia, Kyty, native ports, Battle.net, Xodus, RomM, console portals, Faugus.
5. Sunshine export.
6. Save restore blocking while an emulator runs; recorder lock coordination.
7. Fuzz targets for the archive, save manifest and ROM-header parsers.

### Exit criteria

- A backup made by Omakade 1.18 imports and restores correctly, and a backup made by the port
  is readable by a documented reference test.
- A restore interrupted at each checkpoint is recoverable and never overwrites unexpected
  external changes.
- Fuzzers run for a bounded time in CI with no crashes, hangs or leaks.
- All source adapters have a fixture and a missing-source test.

---

## 9. Phase 6 — Controller and Game Mode (0.9)

### Goal

Controller-first fullscreen play on KineticWE, with graceful fallback elsewhere.

### Tasks

1. SDL 3 controller polling integrated with the GLib main loop; hotplug and reconnect.
2. Focus engine driven by rendered geometry; held repeat; keyboard/controller handoff.
3. Game Mode layout on the same window; header, cards, footer hints.
4. KineticWE adapter:
   - capability probe;
   - output enumeration and selection;
   - window/workspace move and restore;
   - placeholder;
   - bundled KWin script loader;
   - state journal and recovery.
5. Audio routing adapter (PipeWire; `pactl` fallback).
6. Noctalia DND adapter with save/restore.
7. Idle inhibition.
8. Launch hold and warm Steam (behind settings).
9. `--game-mode`, `--game-mode-exit`, `--game-mode-toggle`, `--couch`, F11.
10. Controller navigation contract tests.

### Exit criteria

- Enter and leave Game Mode restores window, workspace, output, audio and DND state.
- A crash during Game Mode is recovered on next start.
- Every directional link is reachability-tested with keyboard and SDL virtual pad.
- On a compositor without a capability, the app says so and leaves the desktop unchanged.
- No KWin script is loaded from a user-controlled path or built from untrusted strings.

---

## 10. Phase 7 — Optional recorder and guide decision (1.0)

### Goal

Decide and, if approved, build highlights. Document the missing guide overlay rather than
building a broken one.

### Tasks

1. **Guide decision.** Record that KineticWE does not provide layer-shell, so the in-game guide
   is unavailable. No overlay code is written. The UI and help state this. Revisit only if the
   compositor gains a supported overlay surface.
2. Guide button evdev helper for actions that do not need an overlay (Game Mode, desktop,
   clip), permissions and udev documentation.
3. Optional recorder:
   - decide external recorder vs ScreenCast/PipeWire;
   - consent and recording indicator;
   - bounded buffer and disk output;
   - achievement clips.
4. Steam achievement clips and clip library, if the recorder lands.

### Exit criteria

- The guide is documented as unavailable on KineticWE; no overlay dependency or code ships.
- If a Home/Capture action exists, it works without claiming to open an overlay.
- If the recorder is unavailable, `--clip` fails with an actionable message and no recording
  starts.
- Optional features can be disabled at build and runtime.

---

## 11. Phase 8 — Hardening and release

### Tasks

- Performance: 10,000-game library, 1,000 covers, startup, memory, scan time.
- Accessibility audit and fixes.
- Full fuzzing, sanitizers, static analysis, CodeQL, dependency review.
- Documentation: README, HELP, CHANGELOG, packaging docs, privacy.
- RPM spec, COPR build, source archive, SBOM, checksums, signing.
- Release candidate acceptance on KineticWE/Noctalia and a fallback session.
- Security sign-off and release notes.

### Exit criteria

See [19-Acceptance criteria](19-ACCEPTANCE-CRITERIA.md).

---

## 12. First ten pull requests

1. `build: add root CMake, GResource, Fedora container and CI skeleton`
2. `app: add Gtk::Application, standard window, header bar and About`
3. `settings: add KeyFile settings with tests`
4. `theme: add ThemeWatcher and standard stylesheet`
5. `db: add sqlite3 wrapper, migrations and schema skeleton`
6. `paths: add XDG resolver and Omakade data import plan`
7. `model: add SourceId, GameKey, Installation, Game`
8. `steam: port scanner with VDF/ACF fixtures`
9. `library: add unified model, filters and grid`
10. `snapshot: add the GTK snapshot smoke test and CI job`

Each PR has tests, no Qt, and updates the relevant document when behaviour changes.

---

## 13. Staffing and sequencing

| Role | Responsibility |
| --- | --- |
| Maintainer | scope, security sign-off, KineticWE/Noctalia acceptance, packaging |
| C++/GTK developer | architecture, core, UI, tests |
| Reviewer | security review, parser/limits review, API review |
| Packager | RPM/COPR, systemd, metainfo, release integrity |

If only one developer is available, the order above still applies; reviews can be asynchronous
but security review cannot be skipped.

---

## 14. What not to do

- Do not start with the QML screens or try to translate them one-to-one.
- Do not build the whole schema and all sources before the first UI.
- Do not make Game Mode or highlights a P0 dependency.
- Do not keep a parallel Qt build “for comparison” in the active tree.
- Do not add a privileged helper for anything in this plan.
- Do not accept a feature that needs a shell command or an unbounded archive.
