# AGENTS.md — LGL Games Library

These rules apply to every AI agent or human contributor in this repository. They override
conflicting defaults.

## 1. Status

This repository is in the **design phase**. There is no application code. Do not build,
install, package or claim the app works. The deliverable is documentation until the maintainer
approves Phase 1.

## 2. Plan first

- Read `README.md` and `docs/README.md` before changing anything.
- Propose a plan (goal, files, approach, risks) and get approval before editing.
- A change that alters scope, architecture, security or packaging updates the relevant
  document in the same change.
- Do not delete or rewrite user-authored content without checking it first.

## 3. Security is mandatory

Read `docs/09-SECURITY.md`. The following are never acceptable, even in examples or tests:

- `system()`, `popen()`, `bash -c`, `sh -c` or string-built commands;
- a privileged/root helper, `sudo`, `pkexec`, setuid or capabilities;
- secrets in settings, the database, logs, argv or fixtures;
- unbounded reads, archive extraction, path traversal, symlink following where unsafe;
- loading Noctalia plugins or executing KWin script source from data;
- disabling TLS verification or adding telemetry.

If a task seems to require one, stop and escalate to the maintainer.

## 4. Platform

- Target: **Fedora 44+, KineticWE (KWin-based Wayland compositor) + Noctalia**.
- GTK 4 / gtkmm 4.20+, C++20, CMake, GResource, RPM/COPR.
- Other Wayland sessions are compatibility targets, not design targets.
- The in-game guide overlay is unavailable: KineticWE does not provide `zwlr_layer_shell_v1`
  to clients. Do not add `gtk4-layer-shell` or claim an overlay works.
- Noctalia is a service (notifications/DND/theme); do not load its plugins or write its config.

## 5. Repository layout (when code starts)

- `src/core`: no widget headers; tested without a display.
- `src/platform`: GLib/GIO/Network/Secret/SDL; no widgets.
- `src/ui`: gtkmm widgets.
- `data/`: GResource inputs.
- `tests/`: `g_test` + CTest.
- `packaging/`: RPM/desktop/metainfo/icons/systemd.
- `legacy/qt/`: archived Omakade Qt tree, never built.

## 6. Documentation rules

- Use the numbered docs in `docs/`; keep cross-links valid.
- Mark plans as plans and shipped behaviour as shipped.
- Do not claim features are implemented, tested or released before the gate in
  `docs/19-ACCEPTANCE-CRITERIA.md`.
- No secrets, personal paths, ROMs, keys, saves or real library data in docs or fixtures.
- When correcting a factual claim (for example a compositor capability), update every affected
  document and the decision log.

## 7. Verification

- Docs-only changes: check links, paths and consistency; no build.
- Code changes (after approval):
  - `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build --output-on-failure`
  - GCC and Clang; warnings as errors; ASan/UBSan for security-relevant code.
  - Run `clang-tidy`, `cppcheck`, `clang-format --dry-run -Werror`.
- Never run KineticWE/Noctalia builds or the user’s session; those are separate repositories.

## 8. Reporting

State what changed, what was checked and the exact commands, and what remains unverified.
“It compiles” is not a claim an agent makes without running the build; in the design phase,
say that no build was run.
