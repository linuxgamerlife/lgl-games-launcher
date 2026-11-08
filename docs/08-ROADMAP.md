# 08 — Roadmap and release gates

This roadmap turns the phases in [07-Implementation plan](07-IMPLEMENTATION-PLAN.md) into
releases. Dates are deliberately absent: the project is design-stage and the estimates are
developer-weeks, not calendar promises. A release ships when its gate is met, not when a date
arrives.

---

## 1. Release sequence

| Release | Name | Theme | Phases | Audience |
| --- | --- | --- | --- | --- |
| 0.0.x | Internal | Build, shell, tests | 1 | maintainers |
| 0.1 | Library | Discover, browse, details | 2 | early testers |
| 0.2 | Play | Launch and record | 3 | early testers |
| 0.5 | Organise | Personal library, metadata, backup | 4 | beta |
| 0.7 | Protect | Saves, long-tail sources | 5 | beta |
| 0.9 | Console | Controller, Game Mode | 6 | release candidate |
| 1.0 | Complete | Optional recorder, hardening, packaging | 7–8 | stable |

The version is independent of Omakade’s 1.18.0. Do not imply feature parity at 0.1.

---

## 2. M0 — Design gate (now)

**Deliverable:** this documentation package.

Gate:

- [ ] Feasibility approved.
- [ ] P0 scope approved.
- [ ] Architecture approved.
- [ ] Security design review approved.
- [ ] KineticWE/Noctalia reference versions recorded.
- [ ] Licence and attribution reviewed.
- [ ] Open decisions Q1–Q9 resolved or explicitly deferred.

If this gate fails, no code is written.

---

## 3. M1 — Foundation (0.0.x)

**Deliverable:** standard GTK shell, settings, theme, empty library, CI, snapshot test.

Gate:

- [ ] Builds on Fedora 44 with GCC and Clang.
- [ ] Warnings as errors; ASan/UBSan build works.
- [ ] GTK standard shell checklist complete.
- [ ] `settings_test`, `theme_test`, `paths_test`, `db_test` pass.
- [ ] Snapshot test produces a PNG.
- [ ] No Qt dependency.
- [ ] First security review of the shell and settings.

---

## 4. M2 — Library (0.1)

**Deliverable:** P0 sources, unified grid, search, filters, basic details.

Gate:

- [ ] All P0 scanners have fixtures and missing-source tests.
- [ ] 200-game fixture renders correctly at 480, 800, 1280 and 2560 widths.
- [ ] Search under 200 ms at 1,000 games.
- [ ] Malformed input never crashes or blocks the main loop.
- [ ] Artwork decode is bounded and off the main thread.
- [ ] Snapshot coverage for empty, populated, list, details and console.
- [ ] No High/Critical security finding.

---

## 5. M3 — Play (0.2)

**Deliverable:** launch + session recording + basic statistics.

Gate:

- [ ] Launch works for every P0 source in fixtures.
- [ ] No shell execution anywhere.
- [ ] Session recorder survives restart and refused writes.
- [ ] No duplicate sessions after a daemon restart.
- [ ] Missing launcher/emulator produces an actionable notice.
- [ ] systemd user unit installs and starts.
- [ ] A real short play session is recorded and visible after restart.

---

## 6. M4 — Organise (0.5 beta)

**Deliverable:** organisation, links, artwork editor, metadata, achievements, full stats,
all settings pages.

Gate:

- [ ] Favourites, hidden, tags, collections, filters, links survive restart and backup.
- [ ] Network clients enforce TLS, timeouts, size caps and host policy.
- [ ] Secrets never appear in files or logs (automated secret scan).
- [ ] Offline caches work after one successful fetch.
- [ ] Year in Review exports a valid PNG.
- [ ] Settings and editors pass keyboard and snapshot tests.
- [ ] Fuzz targets for archive/manifest parsers are in CI.

---

## 7. M5 — Protect (0.7 beta)

**Deliverable:** backup/restore, save protection, remaining sources, Sunshine, RomM.

Gate:

- [ ] Omakade 1.18 backup imports and restores.
- [ ] Port backup is readable by the documented compatibility test.
- [ ] Interrupted restore is recoverable at every checkpoint.
- [ ] Restore never follows symlinks or overwrites unexpected changes.
- [ ] All source adapters have fixtures.
- [ ] Save/restore fuzzers pass.
- [ ] Storage limits enforced and reported.

---

## 8. M6 — Console (0.9 RC)

**Deliverable:** controller navigation, Game Mode, KineticWE/Noctalia adapters.

Gate:

- [ ] Controller focus contract passes on 720p, 1080p, ultrawide, 4K and scaled layouts.
- [ ] Game Mode enter/leave restores window, workspace, output, audio and DND.
- [ ] Crash recovery restores the desktop.
- [ ] Unsupported capability degrades with a reason.
- [ ] KWin script and Noctalia IPC validation tests pass.
- [ ] Physical controller test on the reference machine.

---

## 9. M7 — 1.0 stable

**Deliverable:** optional recorder decision, hardening, packaging. The in-game guide remains
unavailable because KineticWE does not provide layer-shell.

Gate:

- [ ] The guide is documented as unavailable on KineticWE; no overlay dependency or code ships.
- [ ] Optional recorder, if shipped, has consent, indicator, limits and a security review.
- [ ] `--clip` fails safely when unavailable.
- [ ] 10,000-game performance budget met.
- [ ] Accessibility audit complete.
- [ ] RPM/COPR build passes, spec validates, metainfo validates.
- [ ] Signed release, checksums, SBOM.
- [ ] Release notes state known limitations and platform differences.

---

## 10. Post-1.0

- In-game guide overlay only if KineticWE gains a supported overlay surface; requires a new
  design and security review.
- Flatpak evaluation, if the sandbox can be made honest.
- Additional distros (Fedora-only until the KineticWE dependency is separable).
- wlroots adapter for other Wayland sessions.
- Optional Noctalia plugin for a library widget, if it can be done without loading
  untrusted code into the app.
- More emulator save layouts and launcher parsers as the ecosystem changes.

Nothing post-1.0 is promised in the 1.0 docs.

---

## 11. Release cadence

- Patch releases for parser breakage, security fixes and crash fixes.
- Minor releases for new sources/features behind settings.
- No automatic updater; dnf/COPR handles updates.
- Every release updates: spec version/release/changelog, `gtk` version (or root CMake
  version), metainfo releases, `CHANGELOG.md`, README current release.
- Security releases follow [09-Security](09-SECURITY.md) response rules.

---

## 12. Communication

- README states current status and tier.
- Each release note lists: new sources, known gaps by desktop, data migration notes, security
  fixes, and the exact Fedora/KineticWE/Noctalia versions tested.
- Never describe a planned feature as available.
- Never claim parity with Omakade without the matrix in
  [06-Feature parity](06-FEATURE-PARITY.md).
