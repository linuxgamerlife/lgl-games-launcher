# 22 — Steam-only MVP (sections 1–4)

This document overrides the broader roadmap for the first implementation.

**Scope:** Steam only. Feature areas **1 Game discovery**, **2 Library and organisation**,
**3 Game details and metadata**, **4 Launching**. Everything else is deferred, even if already
documented.

**Target:** Fedora 44+, KineticWE + Noctalia, Wayland, GTK 4 / gtkmm 4.22, C++20, RPM/COPR.

---

## 1. In scope

### 1.1 Steam discovery

- Native Steam roots:
  - `~/.steam/steam`
  - `~/.local/share/Steam`
  - `$XDG_DATA_HOME/Steam`
- Flatpak Steam root:
  - `~/.var/app/com.valvesoftware.Steam/.local/share/Steam`
  - `~/.var/app/com.valvesoftware.Steam/data/Steam`
- Read `steamapps/libraryfolders.vdf` for additional libraries.
- Read `steamapps/appmanifest_*.acf` for installed games.
- Read `userdata/<account>/config/librarycache/*.json` for owned games, and
  `appcache/appinfo.vdf` for their names and types, so uninstalled owned games
  appear too.
- Fields: app id, name, install dir, library path, manifest path, installed state,
  last played, playtime minutes, size on disk.
- Exclude Steam runtimes/tools, DLC, soundtracks and entries without a usable name.
- Local artwork: `librarycache`, recursive `appcache/librarycache/<appid>/<hash>/`
  assets, and `userdata/.../config/grid`.
- Missing portrait covers are fetched from Steam’s public CDN (`shared.steamstatic.com`) using only the numeric app id, then cached locally. No Steam Web API, no owned-library view, no metadata network calls.

### 1.2 Library and organisation

- Responsive cover grid for installed and uninstalled owned games.
- Search.
- Sort: title, recently played, playtime.
- Favourites.
- Hidden games with “show hidden”.
- Tags (comma-separated, local).
- Persist flags in SQLite at `$XDG_DATA_HOME/lgl-games-launcher/library.sqlite3`.
- Custom artwork and collections are **not** in this MVP.

### 1.3 Game details and metadata

- Rich details page: hero art, portrait cover, title, source, ProtonDB and PCGamingWiki
  links, install state, Play/Favourite/Hide/Manage in Steam.
- Organisation: status (Backlog/Playing/Completed/Abandoned), tags.
- Stats: playtime, achievement count, completion percentage.
- Steam achievements from the public Steam Community XML, with progress and per-achievement
  status.
- IGDB insights when the user supplies Twitch developer credentials: critic score, rushed,
  main + extras and completionist estimates. The secret is stored in Secret Service.
- SteamGridDB and the metadata editor are **not** in this MVP.

### 1.4 Launching

- Native Steam: `steam -applaunch <appid>`.
- Flatpak Steam: `flatpak run com.valvesoftware.Steam -applaunch <appid>`.
- No shell; argv only.
- Launch failure notice. Uninstalled games open Steam's install prompt.
- Missing Steam or missing install path is an actionable error.
- Single instance: a second launch presents the existing window.
- `--play APPID` optional command line.

---

## 2. Out of scope for this build

- Heroic, Lutris, GOG, Faugus, Battle.net, Xodus, RomM, emulators, ROM folders, native ports,
  manual games.
- Consoles and console cards.
- Play-session recording, `sessiond`, statistics, Year in Review, Discord presence.
- Game Mode, controller navigation, display/audio switching, DND.
- In-game guide and highlights.
- Save protection and library backup/restore.
- Sunshine/Moonlight export.
- Achievements, IGDB, ProtonDB, SteamGridDB, Steam Web API.
- Collections, saved filters, bulk organisation, game links, artwork editor.
- Custom app icon packaging artwork beyond a simple placeholder.

These remain in the design docs for later phases. They must not be presented as available.

---

## 3. Acceptance

- Configure/build in `fedora-dev1`.
- Discovers a real Steam library and a fixture library.
- Grid renders, search/filter/sort work, details open, Play launches Steam.
- Favourites/hidden/tags survive restart.
- Missing Steam, malformed VDF/ACF and missing artwork do not crash.
- No shell execution, no secrets. The only network request is the fixed Steam CDN cover fallback.
- Build passes with warnings as errors.
- Automated tests are deferred for this MVP; the maintainer tests manually.
- A snapshot smoke test may be added later if GTK capture is worth the setup.

---

## 4. Packaging

- Package/binary/icon/config: `lgl-games-launcher`.
- App id: `com.linuxgamerlife.lgl-games-launcher`.
- Display: `LGL Games Launcher`.
- Namespace: `gameslauncher`.
- CSS prefix: `gl-`.
- Env/CMake prefix: `LGL_GAMES_LAUNCHER`.
- Data: `$XDG_DATA_HOME/lgl-games-launcher/`.
- Config: `$XDG_CONFIG_HOME/lgl-games-launcher/settings.ini`.
- Cache: `$XDG_CACHE_HOME/lgl-games-launcher/`.

Only the Steam MVP paths are implemented now.
