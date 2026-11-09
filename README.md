<div align="center">

<img src="packaging/icons/256x256.png" width="128" alt="LGL Games Launcher">

# LGL Games Launcher

**Your Steam library in one clean GTK window for Fedora.**

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

A native GTK 4 launcher for the Steam games already on your machine. It reads Steam's local
library data, shows installed and owned-but-uninstalled games together, and starts games
through Steam or the Flatpak Steam client.

This is the **Steam-only MVP**. It is deliberately **local-ownership-only**: it discovers
what Steam has recorded locally and does not query the Steam Web API. Family-shared or
account-only entries that Steam has not written locally will not appear until optional Steam
Web API owned-library sync is added in a later release.

- One library for installed and uninstalled owned Steam games.
- Local artwork first, with a cached Steam CDN fallback for missing portrait covers.
- Search, sort, favourites, hidden games, tags, installed-only filtering and a Home action.
- Rich details page with ProtonDB and PCGamingWiki links.
- Optional IGDB insights: critic score, rushed, main + extras and completionist estimates.
- Steam achievement progress and achievement list from the public Steam profile XML.
- Mouse Back/Forward and `Alt+Left`/`Alt+Right` history navigation.
- No account requirement for browsing, no telemetry and no background recording.

The current package release is **0.0.1-1**.

---

## Install

LGL Games Launcher is published from the same COPR as the rest of the LGL toolkit:

```bash
sudo dnf copr enable linuxgamerlife/lgl-toolkit
sudo dnf install lgl-games-launcher
```

Launch **LGL Games Launcher** from the application menu. The current package release is
**0.0.1-1**.

---

## Features

| Area | What it does |
|---|---|
| **Steam discovery** | Reads `libraryfolders.vdf`, installed `appmanifest_*.acf`, `localconfig.vdf`, the per-account library cache and `appinfo.vdf` for names and types |
| **Library** | Responsive cover grid, title/recent/playtime sorting, search, source status and installed-only filter |
| **Organisation** | Favourites, hidden games with Show hidden, tags, Backlog/Playing/Completed/Abandoned |
| **Home** | Clears search, Favourites, Installed only and Show hidden in one click (`Ctrl+H`) |
| **Details** | Hero artwork, portrait cover, install state, Play/Install in Steam, Manage in Steam, copy title, copy app ID, ProtonDB and PCGamingWiki links |
| **IGDB** | Optional Twitch developer credentials; critic score and time-to-beat estimates, cached in memory for the session |
| **Achievements** | Public Steam profile XML; progress bar, unlocked/locked rows, Steam achievement icons cached locally |
| **Cover art** | Local Steam cache first, including hashed asset subdirectories, then the Steam CDN; clean NO ART placeholder |
| **Navigation** | Mouse Back/Forward, `Alt+Left`/`Alt+Right`, library/details history |
| **Desktop** | Standard LGL GTK header bar, system title bar mode, About, notices, single instance |

### Deliberately not included yet

- Other launchers: Heroic, Lutris, GOG, Faugus, Battle.net, Xodus Nexus, RomM.
- Emulators and ROM folders.
- Play-session recording, statistics or Discord presence.
- Game Mode, controller navigation or the in-game guide overlay.
- Save protection, library backup/restore or highlights.
- Steam Web API owned-library sync, IGDB installation, or any account login.

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
  desktop-file-utils appstream
```

Automated tests are deferred for this MVP. The code is built with warnings as errors and
has been reviewed with clang-tidy, cppcheck and ASan/UBSan.

---

## Project layout

```text
src/core/     Steam discovery, settings, flags database, IGDB, achievements, launch
src/ui/       GTK window, library grid, details page, theme watcher
data/         stylesheet, app icon, NO ART placeholder, GResource manifest
packaging/    RPM spec, desktop file, metainfo, icons
```

This is an independent GTK port of [Omakade](https://github.com/btsouth/omakade)
1.18.0. The upstream Qt/QML implementation is reference only and is not built.

---

## License

GPL-3.0-or-later. See [LICENCE.md](LICENCE.md). Omakade is Copyright (C) 2026 Tyler South.
Console artwork retains its separate CC BY-SA attribution where present.

---

<div align="center">
Made for <a href="https://fedoraproject.org">Fedora</a> · by
<a href="https://www.youtube.com/@linuxgamerlife">LinuxGamerLife</a>
</div>
