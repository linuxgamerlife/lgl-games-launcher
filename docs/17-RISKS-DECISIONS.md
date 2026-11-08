# 17 — Risks, decisions and open questions

This is a living register. A decision is binding until superseded by a later ADR with a date
and reason. A risk without an owner and mitigation blocks the phase it affects.

---

## 1. Architecture decisions (ADR summary)

### ADR-001 — Native gtkmm 4 rewrite

**Decision:** build the maintained application with GTK 4 + gtkmm 4.20+, C++20, libadwaita
only for initialisation/style management. Archive the Qt/QML tree under `legacy/qt/`.

**Alternatives:** keep Qt under a GTK skin; embed QML in a GTK window; run a Qt service behind a
GTK UI.

**Reason:** the LGL GTK standard, Fedora packaging, theme integration and maintainability. A
hybrid would keep Qt/KF6 dependencies, two UI models and a larger attack surface.

**Consequences:** the QML rewrite is the largest cost. No Qt is a release gate.

### ADR-002 — KineticWE + Noctalia is the design target

**Decision:** design and accept on Fedora with KineticWE (KWin fork) and Noctalia. Other
Wayland sessions are compatibility targets with explicit degradation.

**Alternatives:** GNOME primary; stock KDE primary; Hyprland primary.

**Reason:** the project is for KineticWE/Noctalia. KWin provides the compositor APIs and
Noctalia provides notifications/DND/GTK theme generation.

**Consequences:** KineticWE does not provide layer-shell, so the in-game guide is unavailable
(ADR-008). GNOME/KDE support is not a release blocker but must not crash.

### ADR-003 — Reuse the SQLite schema and backup format

**Decision:** keep the upstream database tables/identity and `.omakade-backup` v1/v2
compatibility; import a copy of an existing library rather than writing in place.

**Reason:** users can migrate; the schema already encodes years of behaviour and recovery
semantics.

**Consequences:** migrations and a schema diff are required; the port cannot casually rename
tables.

### ADR-004 — `settings.ini` with a TOML importer

**Decision:** new settings use `Glib::KeyFile` at
`$XDG_CONFIG_HOME/lgl-games-launcher/settings.ini`; import `omakade/config.toml` once.

**Reason:** LGL GTK standard. TOML remains only as an import format.

### ADR-005 — Separate session daemon

**Decision:** keep `lgl-games-launcher-sessiond` as a systemd user service with its own SQLite
connection, lock and durable journal.

**Reason:** recording must continue with the window closed and must not stall the UI.

### ADR-006 — No privileged helper, no root

**Decision:** the app, daemons and helpers run as the user. No setuid, polkit action, sudo or
root helper.

**Reason:** the feature set does not need privilege. Any future privileged feature needs a new
review under the Privileged Operations Standard.

### ADR-007 — Argv-only process execution

**Decision:** one `LaunchPlan`/`ProcessRunner`; no shell, ever. User manual commands are parsed
without a shell.

**Reason:** the largest code-execution risk in a game launcher. Enforced by tests and review.

### ADR-008 — No in-game guide on KineticWE

**Decision:** do not build a GTK layer-shell overlay. KineticWE does not provide
`zwlr_layer_shell_v1` to clients, so `gtk4-layer-shell` cannot be used. The upstream guide is
dropped from the target feature set.

**Alternatives:** assume the source implementation is usable; use a Noctalia plugin; use a
normal fullscreen window.

**Reason:** a protocol implementation in the source tree is not a supported client surface.
Shipping an overlay that cannot be created is worse than not shipping it.

**Consequences:** the guide is documented as unavailable. `gtk4-layer-shell` is not a build or
packaging dependency. A future guide requires KineticWE to expose a supported overlay surface
and a new design/security review. A normal fullscreen window is not a substitute.

### ADR-009 — Noctalia is a service, not a framework

**Decision:** consume Noctalia notifications/DND and the generated GTK theme. Do not load its
plugins, write its config, or embed its shell.

**Reason:** Noctalia plugins are trusted Luau code; the app has no business executing them.

### ADR-010 — Highlights are deferred

**Decision:** no built-in `gpu-screen-recorder` assumption. Highlights are P3 and require a
separate recorder design (external tool or ScreenCast/PipeWire) and security review.

### ADR-011 — Flatpak deferred

**Decision:** RPM/COPR is the 1.0 packaging. Flatpak is evaluated later, without claiming a
sandbox benefit the app cannot honour.

### ADR-012 — GPL-3.0-or-later

**Decision:** the port is GPL-3.0-or-later, preserves Omakade’s copyright and console artwork
attribution, and does not claim endorsement.

---

## 2. Risk register

Probability/impact: H/M/L. Owner is the role, not a person.

| ID | Risk | P | I | Mitigation | Owner | Trigger |
| --- | --- | --- | --- | --- | --- | --- |
| R-01 | QML→GTK rewrite takes longer than estimated | H | H | Phase MVP; reusable components; no pixel parity | dev | Phase 2 overruns by 30% |
| R-02 | KineticWE lacks layer-shell, so the guide overlay cannot ship | H | H | Document the gap; no overlay dependency; upstream request | maintainer | Confirmed target limitation |
| R-03 | KWin scripting API changes/breaks | M | H | Capability probe; bundled script; fail closed; version pin | dev | KineticWE update |
| R-04 | Launcher formats change | H | M | Fixtures; per-source isolation; keep old cache | dev | User report |
| R-05 | Save restore damages progress | L | H | Journal, `O_NOFOLLOW`, hashes, tests, confirmation | dev | Any restore bug |
| R-06 | Backup parser vulnerability | M | H | Bounds, fuzz, no extraction, review | dev | Fuzz/code review |
| R-07 | Secret leak in logs/diagnostics | M | H | Secret Service, redaction, secret scan | dev | Any finding |
| R-08 | Session recorder duplicates/loses time | M | M | Monotonic time, lock, journal, tests | dev | Recorder bug |
| R-09 | Noctalia IPC changes | M | M | Capability probe, fixed commands, fallback | dev | Noctalia update |
| R-10 | Noctalia/GTK theme mismatch | M | L | Follow GTK, ThemeWatcher, no palette copy | dev | Theme bug |
| R-11 | Fedora dependency changes | M | M | Pin Fedora 44 container; CI | packager | Build failure |
| R-12 | Upstream Omakade diverges | H | M | Treat as reference; no blind merges; document source commit | maintainer | Upstream release |
| R-13 | License/attribution mistake | L | H | Keep COPYRIGHT/LICENSE/ARTWORK; legal review | maintainer | Packaging |
| R-14 | Scope creep (streaming, store, social) | H | M | Non-goals; tier gates | maintainer | New feature request |
| R-15 | Performance with 10k games | M | M | Benchmark; virtualized list; lazy art | dev | Budget miss |
| R-16 | Gamepad permissions vary | M | L | Document; skip; no udev world access | dev | User report |
| R-17 | Flatpak user expectation | M | L | State RPM-only; document why | maintainer | Requests |
| R-18 | AI-generated code defect | M | H | Tests, static analysis, sanitizers, review | reviewer | Tool finding |
| R-19 | Recorder consent/privacy bug (P3) | L | H | Separate review; indicator; opt-in | dev | Feature proposed |
| R-20 | Concurrent Omakade + port library use | M | M | Detect/warn; one-way import; document | dev | User report |

---

## 3. Open questions

| ID | Question | Default if unresolved | Needed by |
| --- | --- | --- | --- |
| Q1 | If KineticWE later provides a supported overlay surface, what design and security review is required for the guide? | Guide stays unavailable | Post-1.0 |
| Q2 | Which KWin API is stable enough: D-Bus methods only, or bundled script too? | Script for window moves, D-Bus for outputs/desktops | Phase 6 |
| Q3 | Is the app id `com.linuxgamerlife.lgl-games-launcher` approved? | Use it | Phase 1 |
| Q4 | Is Game Mode a 1.0 requirement? | Yes for current display; dedicated display conditional | Phase 6 |
| Q5 | Are achievements required for 1.0? | P1, not a 1.0 blocker | Phase 4 |
| Q6 | Is Flatpak a goal? | Deferred | Packaging |
| Q7 | External recorder or ScreenCast/PipeWire for highlights? | Deferred; `--clip` unavailable | P3 |
| Q8 | GPL notice wording | Preserve upstream + add port copyright | Phase 1 |
| Q9 | Should the guide-button helper ship for Home/Capture actions that do not need an overlay? | Optional in Phase 7 | Phase 7 |
| Q10 | Does the project ship a Noctalia plugin for a library widget? | No for 1.0 | Post-1.0 |

---

## 4. Decision log

| Date | Decision | Reason | Supersedes |
| --- | --- | --- | --- |
| 2026-11-08 | Stage a native gtkmm port | Feasibility audit | — |
| 2026-11-08 | Target KineticWE + Noctalia | Correct platform, not GNOME/KDE | earlier draft assumption |
| 2026-11-08 | Layer-shell overlay is unavailable because KineticWE does not provide the protocol | Confirmed target limitation; do not ship an overlay | earlier “unverified/spike” draft |
| 2026-11-08 | No privileged helper | Feature set does not need one | — |

When a decision changes, update this table, the affected docs and the feature matrix in the
same pull request. Do not leave contradictory statements in the suite.
