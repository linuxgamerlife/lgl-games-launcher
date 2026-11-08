# 15 — Repository and data migration

This document follows the LGL
[Qt to GTK Repository Migration Standard](../../docs/qt-gtk-repo%20migration-standard.md).
The target repository is `lgl-games-launcher`; the source is Omakade 1.18.0 at a pinned commit.
The migration has not started.

---

## 1. Target repository layout

```text
lgl-games-launcher/
├── src/                 # maintained GTK application and non-UI C++
│   ├── main.cpp
│   ├── core/
│   ├── platform/
│   ├── ui/
│   └── services/
├── data/                # GResource, CSS, icons, console art, catalogues
├── tests/               # g_test and CTest
├── packaging/           # RPM, desktop, metainfo, icons, systemd units
├── docs/                # current documentation (this suite)
├── legacy/
│   └── qt/              # archived Omakade Qt/QML reference
├── CMakeLists.txt       # builds GTK directly
├── Makefile
├── .copr/Makefile
├── README.md
├── CHANGELOG.md
├── HELP.md
├── COPYRIGHT
└── LICENSE
```

The root is the active GTK app. There is no `gtk/` wrapper.

---

## 2. Archive the Qt implementation

When implementation starts, import the upstream tree at the pinned commit into `legacy/qt/`:

| Upstream item | Archive location |
| --- | --- |
| `src/` | `legacy/qt/src/` |
| `qml/` | `legacy/qt/qml/` |
| `omarchy-plugin/` | `legacy/qt/omarchy-plugin/` |
| `resources/` | `legacy/qt/resources/` |
| `tests/` | `legacy/qt/tests/` |
| `packaging/` | `legacy/qt/packaging/` |
| Qt CMake files | `legacy/qt/` |
| Qt-era docs/screenshots | `legacy/qt/docs/` |

Add `legacy/qt/README.md`:

> Historical reference only. This is Omakade 1.18.0’s Qt/QML implementation. It is not built,
> tested, packaged, or eligible for feature work. The maintained application is the GTK
> implementation in `src/`.

Do not move build directories, caches, `result/`, or generated files into the archive.

Keep the upstream `COPYRIGHT`, `LICENSE`, `PRIVACY.md`, `SECURITY.md` and console artwork
attribution. The port is a derivative work; the GPL notices and CC BY-SA notice must remain.

---

## 3. Promote the GTK build

Root `CMakeLists.txt`:

1. Declare `project(lgl-games-launcher VERSION ... LANGUAGES C CXX)`.
2. Set C++20, warnings-as-errors, options, `CMAKE_EXPORT_COMPILE_COMMANDS`.
3. Find GTK, gtkmm, libadwaita, JSON-GLib, SQLite, libsoup, libsecret, libzip, zstd, OpenSSL,
   SDL3, GdkPixbuf, Cairo, Pango.
4. Compile GResource from `data/`.
5. Build `src/core`, `src/platform`, `src/ui`, `src/main.cpp` and the daemons.
6. Build tests from `tests/` when `BUILD_TESTING`.
7. Install binary, daemons, units, desktop, metainfo, icons, docs and licences.

Remove `add_subdirectory(gtk)` and every Qt-only target. The documented build is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

---

## 4. Source mapping

| Upstream | Port | Notes |
| --- | --- | --- |
| `src/app/main.cpp` (9,486 lines) | `src/main.cpp` + `src/ui/window/*` + `src/ui/pages/*` | Split lifecycle, window, pages, test hooks |
| `src/app/AppSettings.*` | `src/core/settings/*` | `Glib::KeyFile`, TOML importer |
| `src/theme/OmarchyTheme.*` | `src/core/theme/*` + `src/ui/themewatcher/*` | Noctalia-generated GTK theme |
| `src/library/*` | `src/core/library/*` | Typed models; `Gio::ListModel` |
| `src/sources/*` | `src/core/sources/*` | Fedora discovery, bounded parsers |
| `src/launch/*` | `src/core/launch/*` + `src/platform/subprocess/*` | `LaunchPlan`, argv |
| `src/tracking/*` | `src/core/tracking/*` + `src/services/sessiond/*` | SQLite, journal |
| `src/backup/*`, `src/saves/*` | `src/core/backup/*`, `src/core/saves/*` | libzip, journals |
| `src/metadata/*`, `src/achievements/*` | `src/core/metadata/*`, `src/core/achievements/*` | libsoup 3 |
| `src/gamemode/*` | `src/platform/compositor/*` | KineticWE adapter |
| `src/input/*` | `src/platform/controller/*` + `src/ui/gamepad/*` | SDL3 + GTK focus |
| `src/guide/*` | `src/services/guide/*` | Not built: KineticWE does not provide layer-shell |
| `src/highlights/*` | `src/services/highlights/*` | P3, only if recorder approved |
| `src/guidebutton/*` | `src/services/guidebutton/*` | evdev read-only; Home/Capture actions without overlay |
| `src/streaming/*` | `src/core/streaming/*` | Sunshine JSON |
| `qml/*` | `src/ui/*` | Rewrite, not translate |
| `omarchy-plugin/*` | none | Dropped; KineticWE has no layer-shell overlay surface |
| `resources/consoles/*` | `data/consoles/*` | Keep attribution |
| `resources/native-ports.json` | `data/native-ports.json` | Keep catalogue, add tests |
| `packaging/*.desktop`, metainfo | `packaging/*` | New app id and paths |
| `tests/*` | `tests/*` | Rewrite for `g_test`; keep fixtures |

---

## 5. Data migration

On first run, if `settings.ini` does not exist and an Omakade installation is found:

1. Read `$XDG_CONFIG_HOME/omakade/config.toml` with a bounded parser.
2. Map keys to `settings.ini` (see [04-Data model](04-DATA-MODEL.md)).
3. Copy `$XDG_DATA_HOME/omakade/library.sqlite3` to the new data path.
4. Copy custom artwork and save sets, preserving structure.
5. Open the copy, run compatibility checks/migrations, verify counts and hashes.
6. Leave the original untouched.
7. Show a notice: imported, source path, and how to roll back.
8. Do not import while an Omakade process is running; if a lock is detected, stop and explain.

Rollback: the new app never writes to `~/.config/omakade` or `~/.local/share/omakade`.
Deleting the new paths returns the user to Omakade unchanged.

Migration is one-way by default. Two-way sync is out of scope; concurrent use of both apps on
the same library is unsupported and must be detected or warned about.

---

## 6. Documentation migration

- Rewrite README for the GTK app and Fedora/KineticWE/Noctalia.
- Split upstream `docs/GUIDE.md` into `HELP.md` (user-facing, packaged) and internal docs.
- Keep upstream privacy/security commitments; update endpoints and data paths.
- Keep `docs/BACKUP-FORMAT.md`, `SAVE-SETS.md`, `RECORDING-COVERAGE.md` as compatibility
  references, updated for the new paths.
- Archive Omarchy-only docs (`GAME-GUIDE.md`, `OMARCHY-SUGGESTION.md`, Hyprland-specific
  sections) under `legacy/qt/docs/`.
- Update all path references; do not leave active docs pointing at `gtk/src/` or Qt classes.
- Keep CHANGELOG history; new entries describe the GTK port.

---

## 7. Packaging migration

- New package name and app id; do not replace the Omakade package.
- Remove all Qt/KF6/LayerShellQt dependencies.
- Add GTK/gtkmm/GLib/SQLite/libsoup/libsecret/libzip/SDL3 dependencies.
- Install new data paths and systemd unit names.
- Exclude `legacy/qt/` from source archives and SRPMs.
- CI commands root at `cmake -S .` and `ctest --test-dir build`.
- Desktop/metainfo validation in `%check`.
- No conflict with Omakade’s files; the two can coexist, though only one should own a library.

---

## 8. Validation before committing the migration

| Check | Expected |
| --- | --- |
| Fresh configure | `cmake -S . -B build-check` succeeds |
| Build | GTK binary builds from root paths |
| Tests | `ctest --test-dir build-check --output-on-failure` passes/skips cleanly |
| Source archive | Contains `src/`, `data/`, `tests/`; excludes `legacy/qt/` |
| RPM | SRPM and binary RPM build |
| Metadata | `desktop-file-validate`, `appstreamcli validate --no-net` pass |
| Reference audit | No active CMake/README/packaging/CI/docs refer to `gtk/src/`, Qt, QML, Omarchy or Hyprland as current |
| Data import | A sanitized Omakade library imports with matching counts |
| Rollback | Original Omakade data unchanged |

Use `git diff --check` before staging. Prefer Git renames where possible. Stage the archive and
promoted GTK files together so the repository never has two active source layouts.

---

## 9. Git and history

- A single migration commit or a small series that moves the Qt tree and adds the GTK root.
- Keep upstream attribution in commit messages and `COPYRIGHT`.
- Do not rewrite upstream history into this repository unless the licence/provenance requires
  it; record the source commit in `docs/15-MIGRATION.md` and README.
- Tags for this project start at its own releases, not Omakade’s.
