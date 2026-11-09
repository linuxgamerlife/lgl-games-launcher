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
  - local Steam artwork, recursively including `appcache/librarycache/<appid>/<hash>/`
    assets, with a cached Steam CDN fallback for missing portraits;
  - installed and uninstalled owned Steam games, including names from `appinfo.vdf`;
  - responsive GTK library grid, search, sort, favourites, hidden games and tags;
  - rich details page with hero art, portrait cover, ProtonDB/PCGamingWiki links, install state,
    favourites, hidden, status and tags;
  - stats cards, Steam achievement progress and list, and optional IGDB insights;
  - IGDB exact-title fallback when a Steam app has no external-game mapping.
  - Steam and Flatpak Steam launch;
  - single-instance GTK application, standard header bar, menu, About and settings;
  - the supplied LGL Games Launcher icon at 48/64/128/256 and in the header/About;
  - cached Steam achievement icons from the public Steam CDN;
  - mouse Back/Forward buttons and Alt+Left/Alt+Right navigate the library/details history;
  - a clean NO ART placeholder when no cover exists.
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
