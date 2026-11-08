# LGL Games Library

**Design documentation for a native GTK 4 game library for Fedora, based on Omakade.**

> **Status: design only.** No application code exists in this repository yet. This
> repository currently contains the project charter, feasibility assessment, architecture,
> security review, implementation plan and release plan. Nothing here has been built.

## What it is

LGL Games Library brings games installed by Steam, Heroic, Lutris, GOG, emulators and
native ports into one local library. It discovers games without modifying the owning
launcher, shows local artwork and metadata, launches games through the launcher or emulator
that owns them, and can record play sessions, protect emulator saves and back up the
personal parts of the library.

It is a Fedora-first application:

- GTK 4 with gtkmm 4, following the [LGL GTK Desktop Application Standard](../docs/gtk-desktop-application-standard.md).
- Wayland first. **KineticWE** (the KWin-based compositor) with the **Noctalia** shell is
  the primary desktop and the platform that defines the feature set. Other Wayland
  sessions are best-effort compatibility, not the design target.
- RPM and COPR packaging, with an optional Flatpak considered later and explicitly not a
  first-release target.
- No root process, no privileged helper, no shell execution and no telemetry.

## Lineage and licence

This project is an independent GTK port of [Omakade](https://github.com/btsouth/omakade)
1.18.0 (GPL-3.0-or-later, Copyright (C) 2026 Tyler South). The Qt/QML implementation is
kept only as historical reference under `legacy/qt/` once the port starts; it is never
built, tested or packaged.

The port is distributed under **GPL-3.0-or-later**. Console artwork carries separate
CC BY-SA 3.0 attribution and must keep its notice. See [LICENCE.md](LICENCE.md) and
[`docs/15-MIGRATION.md`](docs/15-MIGRATION.md).

This is not an official Omakade release and is not affiliated with the Omakade maintainer.

## Feasibility verdict

**Feasible, in stages, with a deliberately narrower first release.**

The library, discovery, launch, tracking, organisation, backup, save-protection, metadata
and statistics features can be ported to gtkmm with no architectural blocker. The parts
that cannot be ported literally are the Omarchy-specific theme, the Omarchy shell in-game
guide, Hyprland window/workspace control and the assumption that `gpu-screen-recorder` is
installed. KineticWE provides the KWin D-Bus, KGlobalAccel, output-management and ScreenCast
surface needed to rebuild Game Mode and recording integrations, and Noctalia provides the
notification/DND IPC and GTK theme generation the application consumes.

**The in-game guide is not available on the target.** KineticWE does not provide the
`zwlr_layer_shell_v1` protocol to clients, so a GTK 4 layer-shell overlay cannot be placed over
a fullscreen game. The guide was an Omarchy shell plugin upstream; it has no replacement on
KineticWE until the compositor exposes a supported overlay mechanism. The project does not
ship a fake or broken overlay.

The full application is large: about 76,000 lines of C++, 19,800 lines of QML and 31,600
lines of tests in the upstream tree. A staged MVP is realistic; one-to-one parity is a
multi-month project. See [`docs/01-FEASIBILITY.md`](docs/01-FEASIBILITY.md).

## Scope at a glance

| Tier | Meaning | Examples |
| --- | --- | --- |
| P0 | First usable release (0.1) | Discover, browse, search, launch and track Steam/Heroic/Lutris/GOG/RetroArch/ROM-folder/manual games; GTK shell; settings; single instance |
| P1 | Beta (0.5) | Organisation, artwork, metadata, links, backup/restore, save protection, remaining emulators, statistics, basic controller Game Mode |
| P2 | 1.0 and later | Advanced Game Mode, achievements, Year in Review, Sunshine, RomM, Xodus/Battle.net, Discord presence |
| P3 | Optional/conditional | Highlight clips with an external recorder; no in-game overlay until KineticWE supports one |
| Dropped | Not portable or not wanted | Omarchy theme files, Omarchy shell plugin, in-game guide overlay (until KineticWE support), Hyprland Lua key-binding injection, Omarchy do-not-disturb switching |

The detailed matrix is in [`docs/06-FEATURE-PARITY.md`](docs/06-FEATURE-PARITY.md).

## Non-negotiable rules

1. **Security is not flexible.** The other standards may be adapted where the toolkit
   differs; the security requirements in [`docs/09-SECURITY.md`](docs/09-SECURITY.md) may not.
2. The GUI never runs as root. If a future feature needs privilege, it must use a separate
   allow-listed helper designed to the
   [Privileged Operations Standard](../docs/privileged-operations-standard.md); the current
   design needs no such helper.
3. External programs are started with an argument vector, never a shell.
4. Every file, archive, network response, IPC message, ROM header and launcher database is
   untrusted input with explicit size and format limits.
5. Secrets live in the Secret Service, never in the database, settings file or logs.
6. Releases are built in CI, signed and published with checksums and an SBOM.

## Read the plan

Start with [`docs/README.md`](docs/README.md), which lists the documents in reading order.

Most useful first:

1. [`docs/01-FEASIBILITY.md`](docs/01-FEASIBILITY.md) — is it possible, and what changes?
2. [`docs/02-SCOPE.md`](docs/02-SCOPE.md) — what the port will and will not do.
3. [`docs/03-ARCHITECTURE.md`](docs/03-ARCHITECTURE.md) — how the GTK application is built.
4. [`docs/06-FEATURE-PARITY.md`](docs/06-FEATURE-PARITY.md) — feature-by-feature disposition.
5. [`docs/09-SECURITY.md`](docs/09-SECURITY.md) — threat model and secure design review.
6. [`docs/07-IMPLEMENTATION-PLAN.md`](docs/07-IMPLEMENTATION-PLAN.md) — phases and order of work.
7. [`docs/08-ROADMAP.md`](docs/08-ROADMAP.md) — milestones and release gates.

## Current state

| Item | State |
| --- | --- |
| Feasibility study | Complete (2026-11-08) |
| Product scope | Draft for approval |
| Architecture | Draft for approval |
| Security design review | Draft for approval |
| Implementation | Not started |
| Target Fedora release | Fedora 44 and later |
| Target desktop | KineticWE + Noctalia (Wayland) |
| Upstream reference | Omakade 1.18.0 |

## Next action

Review the open decisions in [`docs/17-RISKS-DECISIONS.md`](docs/17-RISKS-DECISIONS.md),
approve the P0 scope, then start Phase 1 in
[`docs/07-IMPLEMENTATION-PLAN.md`](docs/07-IMPLEMENTATION-PLAN.md).

Nothing in this repository may be described as built, tested or released until the
corresponding gate in [`docs/19-ACCEPTANCE-CRITERIA.md`](docs/19-ACCEPTANCE-CRITERIA.md) has
been met.
