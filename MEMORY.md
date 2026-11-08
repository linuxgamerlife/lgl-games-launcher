# LGL Games Launcher — project memory

## State

- Building the **Steam-only MVP** in `docs/22-STEAM-MVP.md`: sections 1–4 only (discovery,
  library, details, launch). Everything else is deferred.
- Source lineage: Omakade 1.18.0 (Qt 6/QML, GPL-3.0-or-later, Copyright (C) 2026 Tyler South).
- Target: Fedora 44+, GTK 4/gtkmm 4.20+, C++20, KineticWE (KWin fork) + Noctalia.
- The Qt tree will be archived under `legacy/qt/` when implementation starts; it is never
  built, tested, packaged or used at runtime.
- The maintained GTK app will live at the repository root (`src/`, `data/`, `tests/`), not in a
  `gtk/` wrapper.

## Key decisions

- Native gtkmm rewrite, not a Qt/GTK hybrid; no Qt/KF6 dependency.
- KineticWE + Noctalia is the design target; other Wayland sessions degrade explicitly.
- SQLite schema and `.omakade-backup` v1/v2 are compatibility contracts.
- `settings.ini` replaces `config.toml`; a one-way importer runs on first use.
- Separate `lgl-games-launcher-sessiond` user service with its own SQLite connection and journal.
- No root helper, no shell, argv-only launch, Secret Service only for secrets.
- The in-game guide overlay is unavailable: KineticWE does not provide `zwlr_layer_shell_v1`
  to clients. Do not add `gtk4-layer-shell` or claim an overlay works.
- Highlights deferred; no built-in `gpu-screen-recorder` assumption.
- Flatpak deferred; RPM/COPR is the 1.0 target.
- Noctalia is a service (notifications/DND/theme), not a framework; never load its plugins or
  write its config.

## Document map

- `docs/01-FEASIBILITY.md` — why this is possible and what changes.
- `docs/02-SCOPE.md` — P0–P3 and non-goals.
- `docs/03-ARCHITECTURE.md` — processes, layers, Qt→GTK mapping.
- `docs/04-DATA-MODEL.md` — paths, settings, schema, compatibility.
- `docs/05-UI-SPEC.md` — GTK standard and screens.
- `docs/06-FEATURE-PARITY.md` — every upstream feature’s disposition.
- `docs/07-IMPLEMENTATION-PLAN.md` / `08-ROADMAP.md` — phases and gates.
- `docs/09-SECURITY.md` — mandatory threat model and `SEC-*` rules.
- `docs/10-PRIVACY.md` — data and network endpoints.
- `docs/11-BUILD.md` / `12-TESTING.md` / `13-PACKAGING.md` — engineering.
- `docs/14-PLATFORM-SUPPORT.md` — KineticWE/Noctalia capabilities and fallbacks.
- `docs/15-MIGRATION.md` — repo/data/doc migration.
- `docs/16-CODING-STANDARDS.md` — C++/gtkmm rules.
- `docs/17-RISKS-DECISIONS.md` — ADRs, risks, open questions.
- `docs/18-STANDARDS-COMPLIANCE.md` — clause mapping.
- `docs/19-ACCEPTANCE-CRITERIA.md` — definition of done and release gates.
- `docs/20-GLOSSARY.md` — terms.
- `docs/21-GUIDE-OVERLAY-BLOCKER.md` — why the guide is unavailable on KineticWE.
- `docs/22-STEAM-MVP.md` — the current Steam-only build scope (sections 1–4).

## Reminders for future agents

- Read `AGENTS.md` and `docs/09-SECURITY.md` first.
- Plan and get approval before editing; this is a design repository.
- Do not build/install/package unless the maintainer explicitly asks and Phase 1 is approved.
- Do not add telemetry, shell execution, root helpers or plaintext secrets.
- Do not add `gtk4-layer-shell` or claim the guide overlay works; KineticWE does not provide
  layer-shell.
- Keep upstream attribution and artwork licences.
- Update every affected document when a platform/security fact changes.
