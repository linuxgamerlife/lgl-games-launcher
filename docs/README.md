# Project documentation

This directory is the complete design package for the GTK 4 port of Omakade to Fedora.
The project is in the design phase. Everything is a plan unless a document explicitly says
otherwise.

## Reading order

| # | Document | Purpose | Audience |
| --- | --- | --- | --- |
| — | [README](../README.md) | Project summary, verdict and status | everyone |
| 01 | [Feasibility](01-FEASIBILITY.md) | Audit of the upstream code base and the GTK/Fedora mapping | maintainers, reviewers |
| 02 | [Scope](02-SCOPE.md) | Product definition, tiers, non-goals, open questions | maintainers, users |
| 03 | [Architecture](03-ARCHITECTURE.md) | Process model, layers, components, threading, IPC | implementers |
| 04 | [Data model](04-DATA-MODEL.md) | Paths, settings, SQLite schema, compatibility and migration | implementers |
| 05 | [UI specification](05-UI-SPEC.md) | GTK standard compliance and every screen | UI implementers, testers |
| 06 | [Feature parity](06-FEATURE-PARITY.md) | Disposition of every upstream feature and source | maintainers |
| 07 | [Implementation plan](07-IMPLEMENTATION-PLAN.md) | Phases, work breakdown, exit criteria, first pull requests | implementers |
| 08 | [Roadmap](08-ROADMAP.md) | Milestones and release sequence | maintainers, users |
| 09 | [Security](09-SECURITY.md) | Threat model, trust boundaries, per-feature secure design review | everyone, reviewers |
| 10 | [Privacy](10-PRIVACY.md) | Local data, network requests, retention and deletion | users, reviewers |
| 11 | [Build](11-BUILD.md) | Fedora toolchain, dependencies, commands and CI | implementers |
| 12 | [Testing](12-TESTING.md) | Test strategy, fixtures, fuzzing, sanitizers, UI snapshots | implementers, reviewers |
| 13 | [Packaging](13-PACKAGING.md) | RPM, COPR, desktop, metainfo, systemd, release integrity | packagers |
| 14 | [Platform support](14-PLATFORM-SUPPORT.md) | KineticWE, Noctalia, Wayland, layer shell and fallback sessions | maintainers, testers |
| 15 | [Migration](15-MIGRATION.md) | Source, data, documentation and packaging migration | implementers |
| 16 | [Coding standards](16-CODING-STANDARDS.md) | C++20 and gtkmm rules mapped from the LGL standards | implementers |
| 17 | [Risks and decisions](17-RISKS-DECISIONS.md) | Risk register, ADRs and open decisions | maintainers |
| 18 | [Standards compliance](18-STANDARDS-COMPLIANCE.md) | Clause-by-clause mapping to the LGL standards | reviewers |
| 19 | [Acceptance criteria](19-ACCEPTANCE-CRITERIA.md) | Definition of done and release gates | maintainers, reviewers |
| 20 | [Glossary](20-GLOSSARY.md) | Terms used across the documents | everyone |
| 21 | [Guide overlay blocker](21-GUIDE-OVERLAY-BLOCKER.md) | Why the in-game guide is unavailable on KineticWE | everyone |

## Source standards

The project follows these documents from `/development/projects/docs/`:

- [GTK Desktop Application Standard](../../docs/gtk-desktop-application-standard.md) — the
  toolkit, look, behaviour and project layout.
- [C++ / Qt Development Standard](../../docs/cpp-qt-development-standard.md) — engineering
  rules; the Qt mechanisms are mapped to GLib/gtkmm equivalents in
  [16-Coding standards](16-CODING-STANDARDS.md).
- [Privileged Operations Standard](../../docs/privileged-operations-standard.md) — applies
  if any privileged helper is ever proposed; none is planned.
- [Secure Design Review Standard](../../docs/secure-design-review-standard.md) — satisfied by
  [09-Security](09-SECURITY.md).
- [Release Integrity Standard](../../docs/release-integrity-standard.md) — satisfied by
  [13-Packaging](13-PACKAGING.md).
- [AI-Assisted Development Safety Standard](../../docs/ai-assisted-development-safety-standard.md)
  — satisfied by [12-Testing](12-TESTING.md).
- [Qt to GTK Repository Migration Standard](../../docs/qt-gtk-repo%20migration-standard.md) —
  satisfied by [15-Migration](15-MIGRATION.md).
- [Secure Web Application Development Standard](../../docs/secure-web-application-development-standard.md)
  — the network rules are adopted where they apply to a desktop client.

## Conventions

- `MUST`, `MUST NOT`, `SHOULD` and `MAY` are used with their RFC 2119 meaning.
- Rule identifiers look like `SEC-LAUNCH-03`; they are referenced from tests and pull
  requests.
- Paths in this repository are relative to the repository root.
- Feature names in `code font` match the upstream name or a proposed symbol.
- “Upstream” means Omakade 1.18.0 at the commit pinned in the migration record.
- “Fedora” means Fedora 44 or later unless a document says otherwise.
- All estimates are planning ranges, not commitments. They are re-estimated at the end of
  Phase 1.

## Documentation rules

1. A decision that changes architecture, scope, security or packaging updates the relevant
   document in the same pull request.
2. A security-relevant change updates [09-Security](09-SECURITY.md) before code is merged.
3. A feature cannot move from “design” to “implemented” without its acceptance criteria,
   tests and security review recorded.
4. Documents state what is true today. Plans are labelled as plans; shipped behaviour is
   labelled as shipped.
5. No document contains secrets, real credentials, personal paths, ROM hashes or save data.
