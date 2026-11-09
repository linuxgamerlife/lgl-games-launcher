<div align="center">

<img src="packaging/icons/256x256.png" width="128" alt="LGL Games Launcher">

# LGL Games Launcher

**Your game library in one clean GTK window for Fedora.**

[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-8cd3cb.svg)](LICENCE.md)
[![Fedora](https://img.shields.io/badge/Fedora-44-blue?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![GTK](https://img.shields.io/badge/GTK-4-blue?logo=gtk&logoColor=white)](https://www.gtk.org)
[![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus&logoColor=white)](https://isocpp.org)

<p align="center">
  <a href="https://ko-fi.com/G2G3V70LW">
    <img src="https://storage.ko-fi.com/cdn/kofi6.png?v=6" height="36" alt="Support on Ko-fi" />
  </a>
</p>
</div>

---

## Overview

A native GTK 4 launcher for your Steam, Faugus and Epic libraries. It shows installed and
owned-but-uninstalled Steam games, Faugus-managed Windows titles, and — after an optional Epic
sign-in — the Epic games you own, installed or not, with real Epic cover art.

It is **local-first**: Steam and Faugus data is read from disk, and the only account connection
is Epic, which you enable explicitly. No telemetry and no background recording.

- One library for Steam games, Faugus entries, and the games installed by the Ubisoft
  Connect, EA App and Epic Games clients that Faugus manages.
- Source filter: Faugus shows the store launchers; Ubisoft, EA and Epic show their games.
- Local artwork first, with a cached Steam CDN fallback for missing Steam covers.
- Search, sort, favourites, hidden games, tags, installed-only filtering and a Home action.
- Rich details page with ProtonDB (Steam) and PCGamingWiki links.
- Optional IGDB insights: critic score, rushed, main + extras and completionist estimates.
- Steam achievement progress and achievement list from the public Steam profile XML.
- Optional Epic Games sign-in (OAuth in an embedded login) for owned games, installed or not.
- Epic cover art from Epic's catalog, with cached images.
- Mouse Back/Forward and `Alt+Left`/`Alt+Right` history navigation.
- No account requirement for Steam/Faugus browsing, no telemetry and no background recording.

The current package release is **0.2.0-1**.

---

## Install

LGL Games Launcher is published from the same COPR as the rest of the LGL toolkit:

```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-games-launcher
```

Launch **LGL Games Launcher** from the application menu. The current package release is
**0.2.0-1**.

---

## Features

| Area | What it does |
|---|---|
| **Steam discovery** | Reads `libraryfolders.vdf`, installed `appmanifest_*.acf`, `localconfig.vdf`, the per-account library cache and `appinfo.vdf` for names and types |
| **Faugus discovery** | Reads `games.json` from the native or Flatpak Faugus data directory, including Ubisoft Connect, EA App and Epic Games entries, playtime and last-played dates |
| **Store discovery** | Finds games installed by Ubisoft Connect, EA App and Epic Games inside each Faugus prefix, wherever the store placed them (Ubisoft `uplay_install.state`, Epic manifests, EA install directories and registry); store games launch directly through Faugus's Proton runner |
| **Epic library** | Optional Epic sign-in through Epic's own login page; lists owned games, installed or not, with Epic catalog cover art; the last library is cached locally so it appears immediately while it refreshes; installed games show Play and uninstalled games show Install, both through the Epic client, with a Store Page fallback; session token in the Secret Service |
| **Library** | Responsive cover grid, title/recent/playtime sorting, search, source filter (All/Steam/Faugus/Epic/EA/Ubisoft) and installed-only filter |
| **Organisation** | Favourites, hidden games with Show hidden, tags, Backlog/Playing/Completed/Abandoned |
| **Home** | Clears search, Favourites, Installed only and Show hidden in one click (`Ctrl+H`) |
| **Details** | Hero artwork, portrait cover, install state, Play/Install in Steam, open the store or Faugus, Manage in Steam, copy title, copy app ID, ProtonDB (Steam) and PCGamingWiki links |
| **IGDB** | Optional Twitch developer credentials; critic score and time-to-beat estimates, cached in memory for the session |
| **Achievements** | Public Steam profile XML; progress bar, unlocked/locked rows, Steam achievement icons cached locally |
| **Cover art** | Local Steam cache first, including hashed asset subdirectories, then the Steam CDN; store games are matched to Steam by title and use Steam's official store asset API for real covers and heroes; Faugus covers and icons come from its data directory |
| **Navigation** | Mouse Back/Forward, `Alt+Left`/`Alt+Right`, library/details history |
| **Desktop** | Standard LGL GTK header bar, system title bar mode, About, notices, single instance |

### Deliberately not included yet

- Other launchers: Heroic, Lutris, GOG, Battle.net, Xodus Nexus, RomM.
- Direct Ubisoft or EA account libraries. Only games installed locally by those clients are
  shown; owned-but-uninstalled Ubisoft/EA games need each store's account API. Epic is
  supported through sign-in.
- Emulators and ROM folders.
- Play-session recording, statistics or Discord presence.
- Game Mode, controller navigation or the in-game guide overlay.
- Save protection, library backup/restore or highlights.
- Steam Web API owned-library sync, IGDB installation, or Ubisoft/EA account logins.

---

## Security and privacy

LGL is local-first. Steam and Faugus are read-only: the launcher never writes to their
directories, and it does not query the Steam Web API. No telemetry is collected.

**Epic sign-in.** The login happens on Epic's own page in an embedded WebKit view. LGL never
sees or stores your Epic password. After sign-in, only the authorization code is handled; it is
exchanged directly with Epic's account service. The resulting access and refresh tokens are
stored in the desktop Secret Service (libsecret) under the purpose `epic-session`, never in
`settings.ini`, the SQLite database, the cache, logs or process arguments. The web view's
cookies and site storage are cleared after sign-in, and **Sign out** removes the token and the
cached Epic library.

**Network use.** Epic account, entitlement, catalog and cover requests run only after you
connect Epic. Steam store search and the Steam CDN are used for cover art. IGDB is used only if
you add its connection. Nothing is sent anywhere else, and there is no telemetry.

**Logs and caches.** The optional Epic diagnostic log at
`$XDG_CACHE_HOME/lgl-games-launcher/epic.log` contains state changes, HTTP status codes,
counts and launch URIs — never tokens or passwords. The Epic library cache stores app ids,
titles and cached cover paths only. Both are created with private permissions.

**Launching.** LGL does not run a shell for Steam. Store and Epic launches pass a constructed,
shell-quoted command to Faugus's own runner; titles and other user text are never interpolated
into it. Game ids and paths come from local store data.

---

## Build from source

Build from a checkout of this repository:

```bash
git clone https://github.com/linuxgamerlife/lgl-games-launcher.git
cd lgl-games-launcher
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build -j"$(nproc)"
./build/lgl-games-launcher
```

Build dependencies:

```bash
sudo dnf install cmake gcc-c++ ninja-build pkgconf-pkg-config \
  gtk4-devel gtkmm4.0-devel libadwaita-devel glib2-devel \
  sqlite-devel libsoup3-devel json-glib-devel libsecret-devel libxml2-devel \
  webkitgtk6.0-devel desktop-file-utils appstream
```

Automated tests are deferred for this MVP. The code is built with warnings as errors and
has been reviewed with clang-tidy, cppcheck and ASan/UBSan.

---

## Project layout

```text
src/core/     Steam and Faugus discovery, settings, flags database, IGDB, achievements, launch
src/ui/       GTK window, library grid, details page, theme watcher
data/         stylesheet, app icon, NO ART placeholder, GResource manifest
packaging/    RPM spec, desktop file, metainfo, icons
```

This is an independent GTK port of [Omakade](https://github.com/btsouth/omakade)
1.18.0. The upstream Qt/QML implementation is reference only and is not built.

---

## License

GPL-3.0-or-later. See [LICENCE.md](LICENCE.md) and the full [LICENSE](LICENSE). Omakade is
Copyright (C) 2026 Tyler South.
Console artwork retains its separate CC BY-SA attribution where present.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by
<a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
