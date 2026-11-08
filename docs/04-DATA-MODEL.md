# 04 — Data model, storage and compatibility

This document defines where the application stores data, the settings contract, the SQLite
schema contract and the compatibility rules with Omakade. It is a security-relevant
document: all stored data is untrusted on read.

---

## 1. Storage locations

| Purpose | Path | Mode | Notes |
| --- | --- | --- | --- |
| Settings | `$XDG_CONFIG_HOME/lgl-games-launcher/settings.ini` | 0600 | `Glib::KeyFile`; one group per area |
| Library database | `$XDG_DATA_HOME/lgl-games-launcher/library.sqlite3` | 0600 | SQLite; WAL side files |
| User artwork | `$XDG_DATA_HOME/lgl-games-launcher/artwork/` | 0700 | Content-addressed copies |
| Save sets | `$XDG_DATA_HOME/lgl-games-launcher/save-sets/` | 0700 | Journals and versions |
| Library backups | user-chosen | user | `.omakade-backup` compatible |
| Artwork/metadata cache | `$XDG_CACHE_HOME/lgl-games-launcher/` | 0700 | Bounded, regenerable |
| Runtime socket | `$XDG_RUNTIME_DIR/lgl-games-launcher/control.sock` | 0600 | Directory 0700 |
| Optional clips | `$XDG_VIDEOS_DIR/LGL Games Launcher/` | user | P2/P3 only |
| Logs | stderr/journal | — | No secrets or personal data |

`XDG_*` variables are resolved with GLib. If a variable is unset, use the GLib default
(`~/.config`, `~/.local/share`, `~/.cache`, `$XDG_RUNTIME_DIR` or a per-user fallback).

The application must not write inside launcher directories (`~/.steam`, Heroic, Lutris,
emulator config) except where the user explicitly chooses an export target. Discovery is
read-only.

---

## 2. Settings contract

Settings use `Glib::KeyFile` with groups named for their area. Every key has a typed getter,
a validator and a default. Missing, malformed or out-of-range values fall back to the
default. Every setter saves immediately; a failed save is reported but never fatal.

Shared keys required by the LGL GTK standard:

| Group | Key | Type | Default |
| --- | --- | --- | --- |
| `window` | `systemTitleBar` | boolean | `false` |
| `window` | `width` | integer (480–16384) | 1280 |
| `window` | `height` | integer (360–16384) | 800 |
| `window` | `maximized` | boolean | `false` |
| `window` | `primaryInput` | string (enum) | `library` |

Application groups (proposed; names may change before implementation but types and bounds
are fixed here):

### `library`

| Key | Type | Default | Bounds |
| --- | --- | --- | --- |
| `sort` | enum `title,recent,playtime,rating,popularity,installed` | `title` | enum |
| `coverSize` | integer | 140 | 60–160 |
| `view` | enum `grid,list` | `grid` | enum |
| `showInstalledOnly` | boolean | `true` | — |
| `expandConsoles` | boolean | `false` | — |
| `consoleExpandLimit` | integer | 100 | 10–100000 |
| `preferStandaloneEmulators` | boolean | `false` | — |
| `reducedMotion` | boolean | `false` | — |
| `closeAfterLaunch` | boolean | `false` | — |

### `sources`

One boolean per source (`steam`, `lutris`, `heroic`, `gog`, `faugus`, `xodus`,
`battlenet`, `retroarch`, `pcsx2`, `rpcs3`, `ppsspp`, `ryujinx`, `shadps4`, `cemu`,
`melonds`, `xenia`, `kyty`, `nativePorts`, `dolphin`, `romm`) plus:

| Key | Type | Default | Notes |
| --- | --- | --- | --- |
| `romFolders` | string list (newline in INI) | empty | Max 64 entries, 4096 chars each |
| `consoleLayouts` | string list `id=card|library` | empty | Validated ids |
| `gogLibraryPaths` | string list | empty | Max 64, absolute, no control chars |
| `consolePortalsEnabled` | boolean | `true` | — |

Source enablement has an “auto” state for sources that are detected at runtime. The settings
file records an explicit boolean only after the user changes it; the detector supplies the
default otherwise. This preserves upstream behaviour where a newly installed emulator is
enabled automatically.

### `connections`

| Key | Type | Default | Secret? |
| --- | --- | --- | --- |
| `steamId` | 17-digit string | empty | no |
| `igdbClientId` | 5–64 alphanumeric | empty | no |
| `igdbClientSecret` | — | — | **Secret Service only** |
| `steamApiKey` | — | — | **Secret Service only** |
| `steamGridDbApiKey` | — | — | **Secret Service only** |
| `retroAchievementsUsername` | 2–20 `[A-Za-z0-9_-]` | empty | no |
| `retroAchievementsApiKey` | — | — | **Secret Service only** |
| `rommUrl` | https URL, no credentials | empty | no |
| `rommUsername` | string | empty | no |
| `rommPassword` | — | — | **Secret Service only** |
| `protonDbEnabled` | boolean | `false` | — |
| `protonDbBadges` | boolean | `false` | — |

No secret is ever written to `settings.ini`, the database, a log, a crash report or a command
line. If Secret Service is unavailable, a secret is kept in memory for the session only and
the UI says it will not persist.

### `controls`

| Key | Type | Default |
| --- | --- | --- |
| `couchModeEnabled` | boolean | `false` |
| `couchLibraryView` | enum `detail,grid` | `detail` |
| `couchCoverSize` | integer 60–160 | 180 |
| `homeButtonOpensGuide` | boolean | `true` (only if guide exists) |
| `pauseUnfocusedSessions` | boolean | `false` |

### `gameMode`

| Key | Type | Default |
| --- | --- | --- |
| `outputName` | string (connector) | empty = current |
| `outputDescription` | string (EDID) | empty |
| `sinkName` | string | empty = current |
| `silenceNotifications` | boolean | `true` |
| `warmSteam` | boolean | `false` |
| `holdLaunchers` | boolean | `false` |
| `shortcutEnabled` | boolean | `false` |

### `highlights` (P2/P3)

| Key | Type | Default |
| --- | --- | --- |
| `enabled` | boolean | `false` |
| `clipSeconds` | enum 15,30,60,90,120 | 30 |
| `achievementClips` | boolean | `false` |

### `privacy`

| Key | Type | Default |
| --- | --- | --- |
| `networkArtwork` | boolean | `true` (only public artwork endpoints) |
| `discordPresence` | boolean | `false` |
| `discordClientId` | 5–32 digits | empty |
| `usageReporting` | boolean | **not implemented; always false** |

### `storage`

| Key | Type | Default | Bounds |
| --- | --- | --- | --- |
| `artworkCacheLimitMb` | integer | 512 | 128–8192 |
| `protectRetroArchSaves` | boolean | `true` | — |
| `trackPlaySessions` | boolean | `true` | — |
| `saveSetLimit` | integer | 10 | 1–50 |
| `backupIncludeHistory` | boolean | `true` | — |

### `stats`

| Key | Type | Default |
| --- | --- | --- |
| `period` | enum `year,all` | `year` |

### Migration from `config.toml`

Upstream stores flat `snake_case` keys in `$XDG_CONFIG_HOME/omakade/config.toml`. The port
imports them once, on first run, when `settings.ini` does not exist:

| Upstream key | New group/key |
| --- | --- |
| `reduced_motion` | `library/reducedMotion` |
| `artwork_cache_limit_mb` | `storage/artworkCacheLimitMb` |
| `steam_id` | `connections/steamId` |
| `igdb_client_id` | `connections/igdbClientId` |
| `discord_client_id` | `privacy/discordClientId` |
| `retroachievements_username` | `connections/retroAchievementsUsername` |
| `*_enabled` | `sources/<source>` |
| `rom_folders` | `sources/romFolders` |
| `console_layouts` | `sources/consoleLayouts` |
| `gog_library_paths` | `sources/gogLibraryPaths` |
| `protondb_enabled`, `protondb_badges` | `connections/protonDb*` |
| `close_after_launch` | `library/closeAfterLaunch` |
| `pause_unfocused_sessions` | `controls/pauseUnfocusedSessions` |
| `track_play_sessions` | `storage/trackPlaySessions` |
| `protect_retroarch_saves` | `storage/protectRetroArchSaves` |
| `discord_presence` | `privacy/discordPresence` |
| `stats_period` | `stats/period` |
| `cover_size`, `couch_cover_size` | `library/coverSize`, `controls/couchCoverSize` |
| `couch_library_view` | `controls/couchLibraryView` |
| `library_sort_mode` | `library/sort` |
| `highlight_*` | `highlights/*` |

The importer is bounded: 1 MiB file, 10,000 keys, 4,096-character values, 64 list entries.
It validates exactly as the runtime setters do. A malformed key is skipped with a warning,
not a fatal error. The original file is never modified. A successful import writes
`settings.ini` atomically and records the source mtime/hash so a later run does not import
twice unless the user explicitly asks.

Secrets found in `config.toml` (if any) are moved to Secret Service and the old value is
left untouched; the UI tells the user to delete the old file after verifying the import.

---

## 3. SQLite schema

### 3.1 Opening rules

- One connection per process; GUI and sessiond are separate processes.
- `PRAGMA foreign_keys = ON`.
- `PRAGMA busy_timeout = 5000`.
- `PRAGMA journal_mode = WAL` on disk databases.
- `PRAGMA synchronous = NORMAL`.
- `PRAGMA user_version` is the schema version. Upstream uses version 9; the port continues
  from the same number and adds migrations from there.
- Every migration runs in a transaction and is idempotent where possible.
- A database from a newer version is opened read-only if possible and the user is told to
  upgrade; never silently downgrade or rewrite it.

### 3.2 Table inventory

The upstream database has roughly 45 tables. The port keeps the schema compatible and may
add tables. Existing table names and identity columns must not change.

| Area | Tables |
| --- | --- |
| Source cache | `games`, `installations`, `source_state`, `owned_games`, `lutris_games`, `heroic_games`, `faugus_games`, `battlenet_games`, `xodus_games`, `retroarch_games`, `pcsx2_games`, `rpcs3_games`, `ppsspp_games`, `ryujinx_games`, `shadps4_games`, `cemu_games`, `melonds_games`, `xenia_games`, `dolphin_games`, `manual_games` |
| Organisation | `user_game_flags`, `game_organization`, `collections`, `collection_games`, `saved_filters`, `artwork_overrides`, `automatic_links`, `game_link_members`, `launch_activity`, `launch_preferences`, `launch_setups` |
| Metadata | `game_metadata`, `game_insights`, `achievements`, `achievement_summary` |
| Play history | `play_sessions`, `session_intervals`, `play_baselines`, `session_journal_state`, `session_tombstones`, `session_game_clears` |
| Saves | save-set tables and clip tables owned by `SaveBackups`/`ClipLibrary` |
| ROM/RomM | `romm_catalog`, `gog_configured_roots`, `review_undo` |
| Achievements cache | `retroachievements_consoles`, `retroachievements_hashes`, `retroachievements_hash_refresh` |

The implementation must generate a schema dump from upstream and diff it before Phase 1.
Any intentional difference is recorded in a migration note and tested with a real
Omakade-created database fixture (sanitized, no personal data).

### 3.3 Identity

- Steam: `(source='Steam', runner='', app_id=<app id>)`.
- Heroic: `(source, runner, app_id)` with runner `heroic`/`gog`/`epic`/`amazon` as upstream.
- Emulators: path-based identity where no stable id exists.
- Manual games: generated stable id, never the title.
- User flags and links key on `(source, runner, app_id)`; paths are not identity.
- A path change is an update, not a new game, when the source supplies a stable id.

### 3.4 Migrations

Create `schema_migrations(version INTEGER PRIMARY KEY, applied_at INTEGER, description TEXT)`
if it does not exist. Each migration:

1. Runs in `BEGIN IMMEDIATE`.
2. Checks `user_version`.
3. Applies only additive or documented transforms.
4. Updates `user_version`.
5. Commits; on failure, rolls back and leaves the old version.

A pre-migration copy of the database is written beside it (`library.sqlite3.pre-<version>`)
with mode 0600, bounded to one copy, and removed after a successful verification.

### 3.5 Database safety

- All statements are prepared; no SQL string is built from user data.
- Table and column names are compile-time constants.
- Queries have `LIMIT` where a user-controlled set could be unbounded.
- Long scans are batched; the UI thread never runs a multi-second transaction.
- The database is never repaired destructively; a corrupt file is copied aside and a new
  one is created only after user confirmation.
- A full or read-only filesystem is reported; the app keeps working in memory where it can.

---

## 4. Compatibility with `.omakade-backup`

The port must read and write the existing archive format. The format is a ZIP with
`manifest.json` and content-addressed artwork.

Required compatibility:

| Item | Rule |
| --- | --- |
| `format` | `omakade-backup` |
| Versions | read 1 and 2; write 2 until a documented version 3 exists |
| Tables | allowlist only; reject unknown tables/fields/settings |
| Artwork | `artwork/<sha256>.<png|jpg|webp>`; digest and type must match |
| Limits | 16 MiB manifest, 32 MiB/image, 512 MiB total, 4,096 files, 100,000 records |
| ZIP | Store or Deflate only; no encryption; no absolute paths; no `..`; regular files only |
| Restore | validate into memory; never extract paths from the archive |
| Write | temp file + atomic rename; 0600 |
| History | version 2 rules, including baselines and session IDs |
| Credentials | never included |
| ROMs/saves | never included |

The parser is a security-critical component and must be fuzzed. See
[09-Security](09-SECURITY.md) and [12-Testing](12-TESTING.md).

---

## 5. Save-set format

Save protection stores, per layout:

- a manifest with version, source, game identity, allowed roots, creation time, file list,
  sizes and SHA-256 hashes;
- the files themselves, addressed by the manifest;
- a restore journal containing both the previous and requested sets, written before live
  files change;
- a commit marker.

Limits: 512 MiB and 20,000 files per set; 2 GiB committed storage; 10 versions per layout by
default. Paths must be under an allowed root, must not be symlinks, and are opened with
`O_NOFOLLOW` where the platform permits. A restore never follows a symlink and never deletes
a live save that was not part of the recorded set.

The full behavioural contract is inherited from upstream `docs/SAVE-SETS.md`; the port must
preserve the recovery states and user-facing messages.

---

## 6. Artwork and metadata caches

- Cache keys are content hashes or provider ids, not user titles.
- Cache directories are bounded by bytes and file count.
- Eviction is least-recently used and never removes user overrides.
- A failed download leaves the previous image.
- Images are decoded with hard pixel and byte limits before being stored.
- Metadata responses are cached with a timestamp and a provider version; stale data remains
  usable offline.
- Removing a connection removes credentials, not cached public artwork unless the user asks.

---

## 7. Data classification

| Class | Examples | Handling |
| --- | --- | --- |
| Secret | API keys, passwords, tokens | Secret Service only; never log/store elsewhere |
| Personal | library, play history, favourites, paths | Local; 0600; export only on user action |
| Sensitive local | ROM paths, save files, emulator config | Read-only unless explicitly changing a save |
| Public metadata | titles, cover URLs, provider ids | Cacheable; no account data |
| Untrusted input | launcher files, archives, network, ROM headers | Validate, bound, never execute |

---

## 8. Retention and deletion

- Removing a source removes its cached rows only after confirmation; user organisation is
  preserved or cleanly detached.
- Removing a game from the library hides it; a separate “Clear history” action deletes play
  sessions after confirmation.
- Removing credentials deletes the Secret Service entry and any cached authenticated data.
- Uninstalling the package does not delete user data by default.
- A “Reset library” action requires typing the word `RESET` and backs up the database first.
- Backups are user files; the app never deletes them automatically.

---

## 9. Portability

The library is machine-local. Backups carry paths as they were; restoring on another machine
does not relocate ROMs or game installations automatically. The user can relink installations
in the UI. This matches upstream and must be stated in the UI and help.
