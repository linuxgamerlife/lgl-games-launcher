# LGL Games Launcher help

## Getting started

LGL Games Launcher is a local library for Steam and Faugus on Fedora. It reads the library
data each launcher already keeps on disk. No account sign-in is required, and no library data
is uploaded.

- **Steam:** installed and owned-but-uninstalled games, playtime and achievements.
- **Faugus:** games and launchers stored in Faugus, including Ubisoft Connect, EA App and
  Epic Games.
- **Stores:** games installed by those clients inside a Faugus prefix, found wherever the
  store placed them. Owned-but-uninstalled store games are not listed; that needs the
  store's account API.
- Browsing works offline. IGDB lookups are optional and only run when connected.

If a source is missing, make sure the launcher has been started at least once so it has
written its local library.

## The library

- The grid shows every detected entry with its source label and playtime.
- **Search** matches titles, tags, sources and app ids.
- **Sort** by title, recently played or playtime.
- **Source** filters All sources, Steam, Faugus, Epic, EA and Ubisoft. Faugus shows the
  store launchers; Ubisoft, EA and Epic show the games installed by that store.
- **Installed only** hides Steam entries that are not installed. Faugus entries are always
  installed.
- **Favourites** and **Show hidden** control the personal view.
- **Home** (`Ctrl+H`) clears search, favourites, installed-only, source and hidden filters.

Use the mouse Back/Forward buttons or `Alt+Left`/`Alt+Right` to move through history.

## Game details

- **Play** starts the game through Steam or Faugus. Ubisoft/EA games start through
  Faugus's Proton runner. Epic games start through the Epic client, so ownership and DRM are
  handled by Epic. Installed Epic games show **Play**; owned-but-uninstalled games show
  **Install**, which asks Epic to install the game. **Store Page** opens the game's Epic product
  page if Epic does not accept the install request.
- **Install in Steam** opens Steam's install prompt for an uninstalled game.
- **Open Faugus** brings up the Faugus window for Faugus entries.
- **Manage in Steam** opens the Steam details page.
- **Copy title** copies the game name.
- **Favourite**, **Hide**, status (Backlog, Playing, Completed, Abandoned) and tags are stored
  locally in the personal flags database.
- ProtonDB links are shown for Steam games. PCGamingWiki works for every entry.
- Steam achievement progress comes from the public Steam profile XML. Faugus entries do not
  show achievements.

## Connections

IGDB is optional. Add a Twitch developer client ID and secret in **Settings → Connections**
to see critic scores and time-to-beat estimates. The client ID is stored in `settings.ini`;
the secret is stored in the desktop Secret Service and is never written to the database or
logs.

**Epic Games** is optional. Use **Sign in to Epic** in the same dialog. The Epic login page
opens inside LGL; your Epic password never reaches the application. The session token is
stored in the desktop Secret Service. Once connected, owned Epic games appear in the library,
installed or not, with Epic cover art. The last library is cached locally, so it appears
immediately at startup while LGL refreshes it in the background. Sign out to remove the
token, the cache and the owned entries.

## Troubleshooting

- **A source is not detected.** Start Steam or Faugus once, then use Refresh.
- **A game will not launch.** Check that `steam` or `faugus-launcher` is on `PATH`; Flatpak
  installs are launched through `flatpak run`.
- **No cover art.** Steam covers fall back to the Steam CDN. Store games are matched to Steam by title and use Steam's
  official store asset API, so their real cover and hero are used.
- **IGDB shows an error.** Re-check the client ID and secret, or remove and re-add the
  connection.
- **Epic does not list games.** Sign in again from **Settings → Connections**; check that the
  Secret Service is available.
- **Achievements are unavailable.** The Steam profile must be public; private profiles
  return no achievement data.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+H` | Home: clear filters |
| `Ctrl+Q` | Quit |
| `Alt+Left` | Back |
| `Alt+Right` | Forward |

## About

LGL Games Launcher is GPL-3.0-or-later. It is an independent GTK port of
[Omakade](https://github.com/btsouth/omakade) by Tyler South. The project page and issue
tracker are at <https://github.com/linuxgamerlife/lgl-games-launcher>.
