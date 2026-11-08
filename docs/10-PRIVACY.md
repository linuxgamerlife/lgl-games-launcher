# 10 — Privacy

This document describes exactly what the application stores, what it sends, and how the user
controls it. It is written for users and reviewers. Security controls are in
[09-Security](09-SECURITY.md).

**Default posture: local-first, offline-capable, no telemetry, no account required.**

---

## 1. Local data

| Data | Location | Why | Removable |
| --- | --- | --- | --- |
| Settings | `$XDG_CONFIG_HOME/lgl-games-library/settings.ini` | Remember choices | yes |
| Library database | `$XDG_DATA_HOME/lgl-games-library/library.sqlite3` | Games, organisation, history, caches | yes |
| Custom artwork | `$XDG_DATA_HOME/lgl-games-library/artwork/` | User-chosen images | yes, per item |
| Save sets | `$XDG_DATA_HOME/lgl-games-library/save-sets/` | Emulator save versions | yes, per set |
| Backups | user-chosen path | Portable personal data | yes |
| Artwork/metadata cache | `$XDG_CACHE_HOME/lgl-games-library/` | Fast offline display | yes |
| Clips (P3) | `$XDG_VIDEOS_DIR/LGL Games Library/` | Saved highlights | yes |
| Runtime socket | `$XDG_RUNTIME_DIR/lgl-games-library/` | Single instance, daemons | automatic |
| Logs | stderr/journal | Diagnosis | by the user’s journal policy |

The library database includes:

- game titles, sources, installation paths, last-played times and playtime;
- favourites, hidden flags, tags, collections, saved filters and artwork choices;
- recorded play sessions and imported emulator totals;
- metadata caches (IGDB/ProtonDB/achievements) and
- no credentials and no ROM/save contents.

---

## 2. Credentials

Credentials are stored only in the desktop Secret Service (libsecret):

- Steam Web API key;
- IGDB client secret (the client ID is not secret and is stored in settings);
- SteamGridDB API key;
- RetroAchievements API key;
- RomM password.

The Steam ID, IGDB client ID and RetroAchievements username are identifiers, not secrets, and
are stored in `settings.ini`. Removing a connection deletes the Secret Service entry and
leaves cached public artwork unless the user clears it.

If the Secret Service is unavailable, a credential can be used for the current session only.
The app says it will not be remembered and never writes it to disk as a fallback.

---

## 3. Network requests

No request is made at startup except optional artwork for visible cards when public artwork is
enabled. The user can disable network artwork in Settings → Privacy.

### 3.1 Public artwork and metadata

| Service | When | What is sent | Notes |
| --- | --- | --- | --- |
| Steam CDN (`shared.steamstatic.com`) | A Steam game has no local artwork | Steam App ID | Public artwork only |
| Steam Web API (`api.steampowered.com`) | User stored an API key and opens achievements or refreshes | API key, Steam ID, App ID | Opt-in |
| Steam owned games | User selects “Sync owned library” | API key, Steam ID | Opt-in |
| IGDB (`api.igdb.com`, `id.twitch.tv`) | User supplied Twitch client ID/secret and requests metadata | Client credentials, game title/App ID, platform | Opt-in |
| ProtonDB (`www.protondb.com`) | Badges enabled and details open | Steam App ID | Off by default |
| SteamGridDB (`www.steamgriddb.com`) | User supplied a key and requests portraits | API key, title/App ID | Opt-in; images only |
| RetroAchievements (`retroachievements.org`) | User supplied username/key and opens a game | Username, matched game id | Opt-in; ROM hashes are computed locally |
| Libretro thumbnails (`thumbnails.libretro.com`) | Missing RetroArch box art | Playlist/system and game label | Public artwork |
| GameTDB (`art.gametdb.com`) | Missing GameCube/Wii cover | Disc/game ID | Public artwork |
| Lutris art (`lutris.net`) | Missing Battle.net art | Game slug/id | Public artwork |
| Microsoft Store (`displaycatalog.mp.microsoft.com`, `store-images.s-microsoft.com`) | Xodus game has no local art | Store product ID | Public artwork |
| RomM (user URL) | User configured a server | Credentials, library request | Self-hosted or user-chosen |
| Discord IPC (local) | Presence enabled | Game name, source, start time, running count | Local socket only; no network by the app |

The app never uploads ROMs, ROM hashes, saves, screenshots, game files, installation paths,
library contents or account passwords to these services.

### 3.2 What is never sent

- The full library, play history, favourites, tags, collections or saved filters.
- ROM files, ROM hashes (RetroAchievements matching happens locally; only the matched game ID
  is sent), saves, save states or emulator keys.
- Steam/IGDB/SteamGridDB/RetroAchievements/RomM passwords in a URL or log.
- Noctalia config, shell history, clipboard, notifications or desktop state.
- Any telemetry, crash report or usage statistic.

### 3.3 Transport

All remote requests use HTTPS with the system trust store. Certificate verification is never
disabled. Requests have timeouts and size limits. Redirects are restricted to documented
hosts. See `SEC-NET-*`.

---

## 4. Noctalia and KineticWE

The application is a normal GTK client in a KineticWE/Noctalia session. It:

- receives the GTK theme Noctalia generates and follows it; it does not read Noctalia’s
  private config or copy its palette into a database;
- sends `GNotification` notifications to the session’s notification daemon (Noctalia in the
  reference session);
- calls `noctalia-kwe msg notification-dnd-status` and `notification-dnd-set` to silence and
  restore notifications during Game Mode;
- uses KineticWE’s compositor D-Bus/scripting to move its own window and, only with the user’s
  action, manage Game Mode windows, outputs and audio;
- does not read other applications’ windows beyond what the compositor exposes for the
  documented Game Mode behaviour, and does not store window titles except transiently for
  attribution when the user enables it.

No data from these calls leaves the machine.

---

## 5. Discord Rich Presence

Off by default. When enabled, the app talks to the local Discord client over its Unix socket
and publishes only:

- the game name,
- the source (for example “Steam”),
- the session start time,
- the number of running games.

It never publishes file paths, account names, achievements, playtime totals or other library
data. If Discord is not running, nothing happens. Disabling the setting stops all publishing.

---

## 6. RomM

Optional and off by default. When the user configures a RomM server, the app sends the
credentials and requests the catalogue the user asked for. The URL must be http(s) and cannot
embed credentials. The user can remove the connection and credentials at any time. A RomM
server the user runs locally is not contacted until configured.

---

## 7. Achievements and Xbox

- Steam achievements require a user-supplied API key.
- RetroAchievements requires a username and API key; ROM hashes are matched locally and only
  the matched game ID is sent.
- Xbox achievements come from Xodus Nexus data already on the machine; the app does not log
  into Xbox.
- Disabling a connection stops requests; cached data remains until the user clears it.

---

## 8. Console keys and ROM metadata

To show a Switch icon/title, the app may read `prod.keys`/`title.keys` from the emulator’s own
config folder. The keys are used in memory to decrypt only the small control section of a
dump. They are never copied, written to the app’s config/database, logged or sent anywhere.
Without them, artwork falls back to nearby files and Ryujinx’s cover cache.

ROM headers (`PARAM.SFO`, NSP/XCI, disc IDs) are read locally for identity and saves. No ROM
content is uploaded.

---

## 9. Screenshots, clips and recordings

- The app does not capture the screen by default.
- P3 highlights are opt-in, show an indicator, and write only to the chosen video folder.
- A future recorder must use explicit consent, bounded storage, and no audio unless enabled.
- Year in Review exports a local PNG of the user’s own statistics; it contains no hidden data
  beyond what the preview shows.

---

## 10. Logs and diagnostics

Logs contain operation names, source counts, bounded errors and capability results. They do
not contain secrets, tokens, cookies, ROM keys, save contents or full personal paths unless a
path is needed to explain a failure. A diagnostics export is user-initiated, previewed, and
redacted by default.

---

## 11. Retention

| Data | Retention |
| --- | --- |
| Settings | until changed or the app is reset |
| Library | until the user removes a source/game or resets |
| Play history | until deleted per game or all history |
| Caches | bounded by size; evicted LRU |
| Save sets | 10 versions per layout by default; user can delete |
| Backups | user files; never auto-deleted |
| Clips | user files; never auto-deleted |
| Logs | journal policy |
| Runtime socket | process lifetime |

A “Reset library” action backs up the database first and requires explicit confirmation.

---

## 12. Removal

- Removing the package does not delete user data by default; this is intentional so updates and
  reinstalls preserve the library.
- The user can remove the config, data and cache directories manually.
- Removing a connection removes its Secret Service entries.
- Removing a custom artwork item removes the app’s copy, not the original file.
- Uninstalling does not remove launcher data, ROMs, saves or game files.

---

## 13. Children and sensitive data

The app has no account, chat, advertising or user-generated content service. It does not
profile users. It may show game metadata and achievement names; these are public game data.
Parents should use the desktop’s own controls for game content.

---

## 14. Changes

Any new network endpoint, telemetry, recording feature or credential store updates this
document and [09-Security](09-SECURITY.md) before release. A feature that cannot be described
here without exposing more data than the user expects does not ship.
