# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses semantic
versioning once implementation starts.

## [Unreleased]

### Added

- Design package for the GTK 4 port of Omakade 1.18.0 to Fedora 44 with KineticWE and Noctalia.
- Feasibility assessment, product scope, architecture, data model, UI specification and
  feature-parity matrix.
- Threat model, secure design review, privacy documentation and `SEC-*` requirements.
- Implementation plan, roadmap, build/test/packaging/platform/migration guides.
- Standards compliance matrix, acceptance criteria, decision log and glossary.

### Changed

- Target desktop corrected to **KineticWE + Noctalia**, not Fedora Workstation/KDE.
- In-game guide removed from the target: KineticWE does not provide `zwlr_layer_shell_v1`, so
  no GTK layer-shell overlay is possible. `gtk4-layer-shell` is not a dependency.

### Security

- No application code exists yet. No release has been made.
- Design requires no root/privileged helper, no shell execution, Secret Service credentials,
  bounded input and argv-only process launch.

## [0.0.0] — 2026-11-08

- Initial design documentation. Not an application release.

[Unreleased]: https://github.com/linuxgamerlife/lgl-games-library/compare/v0.0.0...HEAD
[0.0.0]: https://github.com/linuxgamerlife/lgl-games-library/releases/tag/v0.0.0
