# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses semantic
versioning once implementation starts.

## [Unreleased]

### Added

- Steam-only MVP application:
  - native and Flatpak Steam root discovery;
  - `libraryfolders.vdf` and `appmanifest_*.acf` parsing;
  - local playtime from `localconfig.vdf`;
  - local Steam artwork, with a cached Steam CDN fallback for missing portraits;
  - responsive GTK library grid, search, sort, favourites, hidden games and tags;
  - details page with install/library paths, last played, playtime and tags;
  - Steam and Flatpak Steam launch;
  - single-instance GTK application, standard header bar, menu, About and settings.
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
- Implementation narrowed to a Steam-only MVP covering feature areas 1–4.

### Security

- No shell execution; launch uses argv.
- Steam manifests are read-only.
- Personal flags are stored in a local SQLite database.
- No credentials, no network, no telemetry.

## [0.0.0] — 2026-11-08

- Initial design documentation. Not an application release.

[Unreleased]: https://github.com/linuxgamerlife/lgl-games-launcher/compare/v0.0.0...HEAD
[0.0.0]: https://github.com/linuxgamerlife/lgl-games-launcher/releases/tag/v0.0.0
