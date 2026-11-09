# 11 — Build and development environment

Target: Fedora 44, C++20, CMake, gtkmm 4.20+. Build and test in a Fedora container that
matches the target; never rely on the host’s libraries.

The application does **not** link KF6, KWin, Qt or Noctalia. KineticWE and Noctalia are
runtime services reached over D-Bus and a Unix socket, so the build remains a normal GTK app.

---

## 1. Toolchain

| Tool | Minimum | Fedora 44 package |
| --- | --- | --- |
| CMake | 3.20 | `cmake` |
| Ninja | any | `ninja-build` |
| GCC | 14+ | `gcc-c++` |
| Clang | 18+ | `clang` |
| pkg-config | any | `pkgconf-pkg-config` |
| glib-compile-resources | any | `glib2-devel` |
| clang-format | 18+ | `clang-tools-extra` |
| clang-tidy | 18+ | `clang-tools-extra` |
| cppcheck | 2.x | `cppcheck` |
| Python 3 | 3.12+ | `python3` (test scripts only) |

C++20 is required. C++23 is allowed only where both GCC and Clang in the container support it
and the code remains buildable with the C++20 fallback; do not make a C++23 library feature
mandatory for core code.

---

## 2. Runtime and build dependencies

### 2.1 Required

| Dependency | pkg-config | Fedora package | Used for |
| --- | --- | --- | --- |
| GTK 4 | `gtk4` | `gtk4-devel` | Toolkit |
| gtkmm 4 | `gtkmm-4.0 >= 4.20` | `gtkmm4.0-devel` | C++ widgets |
| libadwaita | `libadwaita-1 >= 1.6` | `libadwaita-devel` | `adw_init`, `AdwStyleManager` only |
| GLib / GIO / GObject | `glib-2.0`, `gio-2.0` | `glib2-devel` | Base |
| JSON-GLib | `json-glib-1.0` | `json-glib-devel` | JSON parsing |
| SQLite | `sqlite3` | `sqlite-devel` | Library database |
| libxml2 | `libxml-2.0` | `libxml2-devel` | Steam achievement XML |
| libsoup 3 | `libsoup-3.0` | `libsoup3-devel` | HTTP |
| libsecret | `libsecret-1` | `libsecret-devel` | Secret Service |
| libzip | `libzip` | `libzip-devel` | Backup archives |
| zstd | `libzstd` | `libzstd-devel` | Compressed archives |
| OpenSSL | `openssl` | `openssl-devel` | Hashes where GChecksum is not enough |
| SDL 3 | `sdl3` | `SDL3-devel` | Controller input |
| ~~gtk4-layer-shell~~ | — | — | Not used: KineticWE does not provide layer-shell |
| GdkPixbuf | `gdk-pixbuf-2.0` | `gdk-pixbuf2-devel` | Image decode |
| Cairo | `cairo` | `cairo-devel` | Year in Review drawing |
| Pango | `pango` | `pango-devel` | Text measurement |

### 2.2 Optional

| Dependency | pkg-config | Fedora package | Used for |
| --- | --- | --- | --- |
| libportal | `libportal-gtk4` | `libportal-gtk4-devel` | Global Shortcuts, ScreenCast later |
| GStreamer | `gstreamer-1.0` | `gstreamer1-devel` | Only if a recorder is built |
| libmanette | `libmanette-0.2` | `libmanette-devel` | Alternative controller backend (not default) |

Optional dependencies must have a build option and a runtime fallback. `gtk4-layer-shell` is
not used: the target compositor does not provide layer-shell, and the guide overlay is not
built.

### 2.3 Test and packaging tools

`xorg-x11-server-Xvfb`, `dbus-run-session`, `desktop-file-utils`, `appstream`, `rpmlint`,
`systemd-rpm-macros`, `git`, `ccache`.

---

## 3. Build commands

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Sanitizers:

```bash
cmake -S . -B build-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DLGL_GAMES_LAUNCHER_ENABLE_SANITIZERS=ON
cmake --build build-asan -j"$(nproc)"
ctest --test-dir build-asan --output-on-failure
```

Clang:

```bash
CC=clang CXX=clang++ cmake -S . -B build-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-clang -j"$(nproc)"
ctest --test-dir build-clang --output-on-failure
```

Static analysis:

```bash
clang-tidy -p build src/**/*.cpp
cppcheck --project=build/compile_commands.json \
  --suppressions-list=.cppcheck-suppressions \
  --error-exitcode=1
```

`CMAKE_EXPORT_COMPILE_COMMANDS` is on in the root CMake.

---

## 4. Fedora container

Use `podman` or `toolbox`/`distrobox` with a Fedora 44 image. Example:

```bash
podman run --rm -it -v "$PWD":/src:Z -w /src fedora:44 bash
```

Inside:

```bash
dnf install -y \
  cmake ninja-build gcc-c++ clang clang-tools-extra cppcheck pkgconf-pkg-config \
  glib2-devel gtk4-devel gtkmm4.0-devel libadwaita-devel json-glib-devel \
  sqlite-devel libsoup3-devel libsecret-devel libzip-devel libzstd-devel \
  openssl-devel SDL3-devel gdk-pixbuf2-devel cairo-devel pango-devel \
  python3 git ccache desktop-file-utils appstream rpmlint systemd-rpm-macros \
  xorg-x11-server-Xvfb dbus-run-session
```

Then use the commands in §3. Do not install Qt or KF6; if a dependency pulls them in, the
build is wrong.

For development inside the actual KineticWE session, run the built binary in a normal
terminal. For UI tests without a compositor, use `xvfb-run` and `GDK_BACKEND=x11` or the
headless backend if available. Do not run the full test suite against the user’s live session
without isolating `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME` and `XDG_RUNTIME_DIR`.

---

## 5. CMake rules

Root:

```cmake
cmake_minimum_required(VERSION 3.20)
project(lgl-games-launcher VERSION 0.1.0 LANGUAGES C CXX)
include(CTest)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(src)
```

The project does not use the `gtk/` wrapper: the root builds the maintained GTK application
directly, per the migration standard. `src/CMakeLists.txt` owns options, resources, targets,
install rules and tests.

Rules:

- `-Wall -Wextra -Wpedantic -Werror` for C++ (GCC and Clang). Generated resource C is `-w`.
- `LGL_GAMES_LAUNCHER_ENABLE_SANITIZERS` adds `-fsanitize=address,undefined
  -fno-omit-frame-pointer` and link options.
- `LGL_GAMES_LAUNCHER_VERSION="${PROJECT_VERSION}"` compile definition for About.
- Resources compiled with `glib-compile-resources` from `data/`.
- Install binary, desktop file, metainfo, hicolor icons, user units, docs, licences.
- `install(DIRECTORY ...)` paths use `GNUInstallDirs`.
- Tests: one executable per core module, registered with CTest; UI/snapshot tests marked as
  skipped (77) when no display is available.
- No network at configure or build time. All inputs are in the tree or Fedora packages.

---

## 6. CI workflow

`.github/workflows/ci.yml` (or the project’s chosen forge) runs in a Fedora 44 container:

1. Install build dependencies.
2. Configure Debug and Release with GCC.
3. Build with warnings as errors.
4. Run `ctest` with a temporary XDG root.
5. Configure/build/test with Clang.
6. ASan+UBSan build and test.
7. `clang-tidy`, `cppcheck`, `clang-format --dry-run -Werror`.
8. CodeQL.
9. Snapshot test and upload PNG artifacts.
10. Build the source archive and SRPM, then validate desktop/metainfo.
11. SBOM and dependency scan.

Nightly/periodic:

- fuzz targets (bounded time, corpus retained);
- sanitized integration tests with a nested compositor where possible;
- COPR build.

CI artifacts are not releases. Release builds follow [13-Packaging](13-PACKAGING.md).

---

## 7. Developer workflow

1. Create a branch.
2. Write the design note and security note for the feature.
3. Add core types and tests.
4. Add the UI or adapter.
5. Run the container build and tests.
6. Run static analysis and sanitizers for security-relevant changes.
7. Update docs, changelog and the feature matrix.
8. Open a pull request with the commands run and their results.

Never commit:

- build directories, `compile_commands.json`, ccache, test outputs;
- real credentials, tokens, ROMs, keys, saves, personal paths or library databases;
- generated resource C;
- a second active Qt build.

---

## 8. Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| `gtkmm-4.0` not found | Wrong container or missing `gtkmm4.0-devel` | Install the Fedora 44 dev package |
| Link error for `adw_*` | libadwaita not linked | Link `libadwaita-1`; it has no C++ binding |
| UI test cannot open display | No compositor/Xvfb | Run under `xvfb-run` or skip |
| Guide overlay unavailable | Expected: KineticWE does not provide layer-shell | No fix; the UI reports it |
| Controller not detected | `/dev/input` permission or SDL backend | Check `SDL_VIDEODRIVER`, seat permissions |
| Settings test touches real config | Test did not set a temp path | Fix the test; never use the real XDG root |
