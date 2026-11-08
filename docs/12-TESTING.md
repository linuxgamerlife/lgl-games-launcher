# 12 — Testing and verification

Testing is mandatory for core logic. The upstream suite is a behaviour specification, not a
code template: the port rewrites tests for GLib’s `g_test` framework and CTest, preserving the
fixtures and expected outcomes.

---

## 1. Goals

- Prove the security boundaries in [09-Security](09-SECURITY.md).
- Prove compatibility with existing Omakade data and backups.
- Prove launch behaviour without launching real games in CI.
- Prove the UI is usable with keyboard, mouse and controller at multiple sizes.
- Catch regressions in parsers, save handling and the database.
- Keep tests deterministic, isolated and free of personal data.

---

## 2. Layers

| Layer | What | Tool | Runs |
| --- | --- | --- | --- |
| Unit | pure functions, parsers, validators, settings, schema | `g_test` | every build |
| Core integration | scanners + SQLite, backup, saves, recorder | `g_test` + temp dirs | every build |
| Process integration | argv, subprocess, single instance, daemon | `g_test` + fake programs | every build |
| UI model | filters, sort, selection, focus contract | `g_test` + offscreen GTK | CI |
| UI snapshot | rendered windows | `LGL_GAMES_LAUNCHER_GTK_SNAPSHOT` + Xvfb | CI |
| Security | malicious input, limits, redaction | `g_test` + fuzz | every build / nightly |
| Performance | 1k/10k libraries, scan, startup | benchmark executable | milestone |
| Manual | real launchers, gamepad, KineticWE/Noctalia | checklist | release gate |

---

## 3. Test layout

```text
tests/
  CMakeLists.txt
  settings_test.cpp
  theme_test.cpp
  paths_test.cpp
  db_test.cpp
  model_test.cpp
  filters_test.cpp
  launch_test.cpp
  subprocess_test.cpp
  ipc_test.cpp
  backup_test.cpp
  backup_recovery_test.cpp
  saves_test.cpp
  save_recovery_test.cpp
  session_test.cpp
  stats_test.cpp
  metadata_test.cpp
  achievements_test.cpp
  artwork_test.cpp
  controller_test.cpp
  game_mode_test.cpp
  kwin_script_test.cpp
  noctalia_ipc_test.cpp
  fuzz/
  fixtures/
```

One executable per module, registered with CTest. Every app has at least `settings_test` and
`theme_test`, as the GTK standard requires.

Tests that need a display, a tool or a session call `g_test_skip()` and are registered with
`SKIP_RETURN_CODE 77`. A skipped test is visible, never silently passed.

---

## 4. Isolation rules

- Every test sets a temporary `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME` and
  `XDG_RUNTIME_DIR` from `g_dir_make_tmp()`.
- No test reads or writes the developer’s real library, config, cache, ROMs or saves.
- Tests use fixtures copied into the temp root; they never mutate the source fixture.
- Network tests use a local HTTP server with a test certificate or a recorded response. No
  test contacts a real provider.
- A test that needs a real launcher/emulator skips when it is absent and says why.
- Tests are deterministic: injected clocks, no sleeps where an event can be awaited, bounded
  timeouts.
- Each test cleans its temp root; a failure may keep it under `LGL_GAMES_LAUNCHER_KEEP_TEST_TMP=1`.

---

## 5. What to test

### 5.1 Settings and theme

- Missing file, unreadable file, malformed group/key/value → default.
- Bounds and enums: invalid values rejected, not clamped silently unless documented.
- Save failure is non-fatal; the setter reports it.
- TOML import: valid keys, malformed keys, size/list caps, secrets not imported into INI.
- ThemeWatcher: debounce, user CSS reload, no-portal fallback, mono class list, `.dark`.
- CSS has no hardcoded colour outside the allowed status colours.

### 5.2 Paths and files

- XDG defaults, relative rejection, `..`, NUL, control chars, overlong paths.
- Symlink/hardlink/loop cases for saves, backups and cache.
- Atomic write: old file preserved on failure.
- Mode 0700/0600 on created directories and files.

### 5.3 Database

- Fresh schema creates all tables; `user_version` advances.
- Opening an Omakade 1.18 database preserves data and migrates.
- A newer database is refused read-write.
- Prepared statements handle quotes, Unicode and NUL.
- Transaction rollback on failure.
- WAL and busy-timeout behaviour with two connections.
- Corrupt database is copied aside and not silently destroyed.

### 5.4 Scanners

For each source: valid fixture, missing file, malformed file, empty file, oversized file,
unexpected type, path with spaces/Unicode, duplicate entries, removed entry, and a source
that is not installed.

Specific parsers:

- Valve KeyValues (VDF/ACF) including nested objects, escapes, comments and truncation.
- Steam `libraryfolders.vdf` and `appmanifest_*.acf`.
- Heroic JSON caches for Epic/GOG/Amazon.
- Lutris YAML with quoting and missing fields.
- Desktop entries and AppImage metadata.
- RetroArch playlists and core names.
- ROM headers: `PARAM.SFO`, NSP/XCI, GameCube/Wii, DS, PSP, PS3, PS4, Xbox, Switch.
- Emulator configs for save layouts.

Fixtures are small, synthetic and redistributable. Never commit a real ROM, key, save or
personal database.

### 5.5 Launch

- Every source builds the expected argv.
- Titles/paths containing spaces, quotes, `$()`, `;`, newline, Unicode and leading `-` are
  single arguments and do not become a command.
- Flatpak ID validation and argv.
- Program not found, not executable, wrong type, missing working directory.
- Environment allowlist; no secret in argv/env.
- Manual command parse: quotes, escapes, empty, control characters, shell metacharacters.
- Launch failure returns exit status and bounded stderr.
- A killed child is observed and the session closes.

### 5.6 IPC and daemons

- Single instance claim and forward.
- Malformed JSON, oversized line, unknown command, wrong peer, connection storm.
- Commands are fixed and cannot launch an arbitrary program.
- Session recorder: open/extend/close, monotonic clock, pause, restart, journal replay,
  second recorder rejected, read-only database, corrupted journal.
- Guide payload schema and fixed action allowlist.

### 5.7 Backup and saves

- Round trip: export → import → identical personal data.
- Omakade v1 and v2 fixtures.
- Unknown table/field/setting rejected.
- Zip slip, symlink, duplicate, absolute path, backslash, device, encrypted, bomb.
- Hash mismatch, wrong image type, oversized image, pixel bomb.
- Restore merge vs replace.
- Interrupted restore at every checkpoint; recovery/undo.
- Save layouts: every emulator, configured roots, flatpak roots, missing saves, empty saves,
  shared banks, emulator running, unexpected external change.
- Limits: 512 MiB / 20,000 files / 2 GiB storage.

### 5.8 Metadata and achievements

- Recorded JSON fixtures for IGDB, ProtonDB, Steam, RetroAchievements, SteamGridDB.
- Malformed/partial/oversized/HTTP error/timeout/redirect.
- Cache preserved on failure; stale data usable offline.
- No credential in a URL or log.
- Image limits and decode failure.

### 5.9 UI and controller

- `Gio::ListModel` filter/sort correctness.
- Search normalisation, debounce, no stale results.
- Selection/focus after filtering, source disable, empty result.
- Keyboard: Tab/Shift+Tab, arrows, Enter, Escape, Ctrl+F, F1, F10, Alt.
- Controller: D-pad and analog, held repeat/release, reconnect, keyboard/controller handoff.
- Focus contract: every directional link checked against rendered positions at 720p, 1080p,
  ultrawide, 4K and 200% scale.
- Responsive layout at 480, 800, 1280, 2560.
- Snapshot: empty, populated, list, details, settings, statistics, Game Mode, dialog.

### 5.10 Game Mode and desktop adapters

- Capability probe with fake D-Bus/Noctalia responses.
- KWin script operation allowlist; invalid UUID/output rejected; no script source from input.
- Enter/leave restores state; interrupted session recovers.
- DND status/set/toggle, save/restore, failure leaves unchanged.
- Output enable/disable confirmation and rollback.
- Guide reported unavailable on KineticWE; no crash, no fake overlay.
- Overlay focus release on hide/exit/crash.

### 5.11 Performance

Budgets (targets, measured on the reference machine):

| Metric | Budget |
| --- | --- |
| Warm start to interactive, 1,000 games | ≤ 1.5 s |
| Cold start to interactive, 1,000 games | ≤ 2.5 s |
| Search keystroke, 10,000 games | ≤ 50 ms |
| Filter/sort rebuild, 10,000 games | ≤ 150 ms |
| Scan 1,000 Steam manifests | ≤ 2 s |
| Main-loop block | ≤ 50 ms |
| Memory, 1,000 covers | ≤ 500 MiB |
| Database open + schema check | ≤ 200 ms |

A benchmark executable uses a generated library and prints results; CI records them and fails
on a large regression. Do not optimise before the benchmark exists.

---

## 6. Fuzzing

Targets:

- Valve KeyValues parser;
- TOML/INI importer;
- JSON schemas for launcher caches;
- `PARAM.SFO` and ROM header parsers;
- backup ZIP + manifest + artwork;
- save-set manifest + journal;
- IPC line parser;
- Noctalia DND output parser.

Use libFuzzer or a simple `g_test` corpus runner. CI runs a bounded time; nightly runs longer.
Any crash, hang, OOM or sanitizer finding is a release blocker. Seed corpora are synthetic.

---

## 7. Security verification

- ASan + UBSan on the full test suite.
- TSan on the recorder and worker-thread tests where practical.
- `clang-tidy` and `cppcheck` with reviewed suppressions.
- CodeQL.
- Secret scan over source, fixtures and build output.
- Dependency review/OSV scan.
- `grep` gate for `system(`, `popen(`, `bash -c`, `sh -c`, `QProcess`, `QString`, `#include <Qt`.
- Tests for every `SEC-*` rule that can be tested.

A High/Critical finding blocks release. A Medium finding needs a decision; a Low finding is
tracked with an owner.

---

## 8. Snapshot and visual testing

`LGL_GAMES_LAUNCHER_GTK_SNAPSHOT=<folder>`:

- run `NON_UNIQUE`;
- wait 3 seconds for rendering;
- render each visible window to `<folder>/<title>.png`, replacing non-alphanumeric characters
  with `-`;
- quit.

Use temporary XDG roots and fixture states. The test checks:

- every expected window produced a non-empty PNG;
- dimensions match the requested size;
- no blank/transparent frame (sample pixels);
- optional perceptual comparison against a checked-in baseline, with a tolerance and a manual
  update path.

Baselines are small and theme-neutral where possible. Do not make CI fail on a font-rendering
difference that is not a layout regression.

---

## 9. Manual acceptance

Automated tests cannot prove real launcher, gamepad or compositor behaviour. The release
checklist includes:

- [ ] Steam native and Flatpak: scan, launch, record, artwork.
- [ ] Heroic Flatpak: scan, launch, record.
- [ ] Lutris native: scan, launch.
- [ ] GOG direct: scan, `umu-run` launch.
- [ ] RetroArch native and Flatpak: scan, core launch, save protection.
- [ ] One ROM-folder console: scan, launch, save protection.
- [ ] One manual game.
- [ ] 8BitDo/Xbox/PlayStation controller: navigation, reconnect, Home/Capture if enabled.
- [ ] 720p, 1080p, 1440p, 4K and 200% scale.
- [ ] KineticWE: Game Mode enter/leave, output, audio, DND; guide reported unavailable.
- [ ] KineticWE: crash during Game Mode → recovery.
- [ ] Noctalia: notifications, DND restore, GTK theme change.
- [ ] Stock KDE/GNOME fallback: app library and launch work; unsupported features explain.
- [ ] A real Omakade library import and a real backup restore.
- [ ] A save restore with an emulator running (must refuse) and with it closed (must succeed).

Record the exact versions in the release notes.

---

## 10. Test data policy

- Only synthetic or clearly licensed fixtures.
- No real ROMs, `prod.keys`, title keys, saves, save states, credentials, personal paths,
  library databases or screenshots of a user’s desktop.
- Fixture filenames and contents are reviewed for personal data.
- Test databases are generated by the test, not committed, except a sanitized schema fixture
  with no rows.

---

## 11. Definition of a good test

- It fails for the bug it describes before the fix.
- It is deterministic and does not depend on network, wall clock or the user’s session.
- It asserts behaviour and limits, not internal implementation details.
- It uses the smallest fixture that exercises the boundary.
- It runs in the normal CTest suite, or skips with a clear reason.
- It leaves no temp files on success.
