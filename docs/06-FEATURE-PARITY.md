# 06 — Feature parity matrix

This is the authoritative disposition of upstream Omakade 1.18.0 behaviour. “Port” means
the behaviour is preserved with GTK/GLib types. “Redesign” means the user outcome is kept
but the implementation is new. “Adapter” means it works only where the desktop provides the
capability. “Defer” means planned after the release named. “Drop” means not ported.

Legend: **P0/P1/P2/P3** are the release tiers in [02-Scope](02-SCOPE.md). A feature’s phase
is the first tier in which it must work, not the only tier where it may be improved.

---

## 1. Library and discovery

| Feature | Upstream reference | Decision | Phase | Implementation notes | Verification |
| --- | --- | --- | --- | --- | --- |
| Unified library of installed games | `UnifiedGameModel` | Port | P0 | Typed `Game`/`Installation`; merge by stable key; one preferred installation | Unit fixtures for every merge case |
| Steam discovery | `sources/steam/SteamScanner` | Port | P0 | `libraryfolders.vdf`, `appmanifest_*.acf`, local artwork; native + Flatpak roots | Fixture VDF/ACF, missing/corrupt files |
| Steam owned games | `SteamOwnedGamesApi` | Port | P1 | Opt-in API key; cache; owned view | Recorded response fixtures; no live network test |
| Heroic discovery | `sources/heroic/HeroicScanner` | Port | P0 | Epic/GOG/Amazon caches; installed and owned; native + Flatpak | Fixture caches, missing fields |
| Lutris discovery | `sources/lutris/LutrisScanner` | Port | P0 | YAML DB; native + Flatpak | Fixture YAML, malformed entries |
| Direct GOG discovery | `sources/gog` + settings | Port | P0 | Standard and extra folders; `umu-run` for Windows builds | Fixture manifests, path validation |
| Faugus discovery | `sources/faugus/FaugusScanner` | Port | P1 | Detect when installed; no Fedora package promise | Fixture DB, absent app |
| Xodus Nexus discovery | `sources/xodus/XodusScanner` | Port | P2 | Shortcut data; external runtime | Fixture DB, malformed rows |
| Battle.net discovery | `sources/battlenet/BattleNetScanner` | Port | P2 | Agent `product.db` in Wine/Proton/Bottles prefix | Fixture DB variants |
| RetroArch discovery | `sources/retroarch/RetroArchScanner` | Port | P0 | Playlists, cores, local thumbnails | Fixture playlists, missing core |
| ROM folder scanning | `sources/retro/RomFolderScanner`, `ConsoleCatalog` | Port | P0 | 45-system catalogue, EmuDeck-style roots, extension allowlist | Catalogue tests, extension/root fixtures |
| PCSX2 discovery | `sources/pcsx2/Pcsx2Scanner` | Port | P1 | Game-list cache, memory-card paths | Fixture cache |
| RPCS3 discovery | `sources/rpcs3/Rpcs3Scanner` | Port | P1 | Installed index, `PARAM.SFO`, category rejection | Fixture SFO |
| PPSSPP discovery | `sources/ppsspp/PpssppScanner` | Port | P1 | `PARAM.SFO` in ISO/PBP, homebrew ELF fallback | Fixture images |
| Dolphin discovery | `sources/dolphin/DolphinScanner` | Port | P1 | Game folders, cache covers, GameTDB | Fixture folders |
| Cemu discovery | `sources/cemu/CemuScanner` | Port | P1 | Title IDs, MLC roots, WUA | Fixture metadata |
| melonDS discovery | `sources/melonds/MelondsScanner` | Port | P1 | ROM header, battery save path | Fixture ROM headers |
| Ryujinx/Eden discovery | `sources/ryujinx/RyujinxScanner` | Port | P1 | NSP/XCI metadata; `prod.keys` read in memory only, never stored/logged | Fixture metadata; secret-leak test |
| shadPS4 discovery | `sources/shadps4/Shadps4Scanner` | Port | P1 | Title saves, profiles, save roots | Fixture tree |
| Xenia discovery | `sources/xenia/XeniaScanner` | Port | P1 | Recent games, `default.xex`, ISO/XEX/ZAR | Fixture list |
| Kyty discovery | `sources/kyty/KytyScanner` | Port | P1 | `param.json`, launcher wrappers, PS5 folders | Fixture tree |
| Native ports | `sources/ports/NativePortScanner`, `PortCatalog` | Port | P1 | Desktop entries/AppImages + bundled catalogue; no downloads | Catalogue and entry fixtures |
| RomM catalogue | `sources/romm/*` | Port | P2 | User URL, credentials in Secret Service, size limits | Mock HTTP; URL validation |
| Console portals | `ConsolePortalModel` | Port | P1 | Group games behind console cards; pin to library | Model tests |
| Source enable/auto | `AppSettings` | Port | P0 | Explicit setting overrides detection; new source auto-enables | Settings tests |
| Source diagnostics | Settings panel | Port | P0 | Per-source status, counts, errors, scan time | UI snapshot + model test |

### Fedora discovery requirements

Every source adapter must:

- Resolve executables with `g_find_program_in_path` and, for Flatpak, known app IDs.
- Search XDG data dirs and Flatpak exports.
- Accept user overrides for unusual installs.
- Treat a missing source as empty, not an error.
- Never write to launcher directories.
- Bound directory traversal (depth, entries, symlink policy) and file sizes.

---

## 2. Browsing and organisation

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Cover grid | Redesign | P0 | `Gtk::GridView`, adaptive columns, lazy artwork |
| List view | Redesign | P0 | `Gtk::ColumnView` |
| Search | Port | P0 | Core normalised title/alias search; debounce 150 ms |
| Source filter | Port | P0 | Multi-select source list |
| Installed / ready to install | Port | P0 | Per-installation availability |
| Sort modes (title/recent/playtime/rating/popularity/installed) | Port | P0/P1 | Rating/popularity need metadata; fall back to title until available |
| Favourites | Port | P1 | `user_game_flags`; optimistic UI with rollback |
| Hidden games | Port | P1 | Hidden from main grid; visible with “Show hidden” |
| Completion status | Port | P1 | Same values as upstream |
| Tags | Port | P1 | Normalised, bounded list |
| Collections | Port | P1 | Create/delete, membership, ordering |
| Saved filters | Port | P1 | Max 500; validated state |
| Bulk organisation | Port | P1 | Multi-select; explicit apply; undo entry |
| Random pick | Port | P1 | Avoid immediate repeat; respects current filter |
| Console cards and layouts | Port | P1 | Card/library per console; expand limit |
| Pinning | Port | P1 | Keep a console game in the main library |
| Launch activity / recently played | Port | P0 | Local launch records |
| Game links / preferred installation | Port | P1 | Link candidates, unlink, one preferred |
| Review undo | Port | P1 | Undo the last organisation change |
| Artwork editor | Redesign | P1 | File picker + crop/preview; content-addressed copy |
| Custom artwork reset | Port | P1 | Remove unused app copy, restore source art |
| Reduced motion | Port | P0 | Setting + CSS/animation behaviour |

---

## 3. Game details and metadata

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Details page | Redesign | P0 | Sections, primary Play, back navigation |
| Local artwork (cover/hero/logo/icon) | Port | P0 | Launcher files, console art, cache |
| IGDB identification and insights | Port | P1 | OAuth client credentials in Secret Service; cache 30 days |
| IGDB critic score / time to beat | Port | P1 | Optional; no account required beyond user credentials |
| ProtonDB badges | Port | P1 | Off by default; App ID only |
| SteamGridDB portraits | Port | P1 | Opt-in key; image limits |
| Steam achievements | Port | P1 | API key; schema/player/global rarity; cache |
| RetroAchievements | Port | P1 | Username + API key; local hash matching; cache |
| Xbox achievements | Port | P2 | Via Xodus data; external runtime |
| Achievement progress UI | Redesign | P1 | List with rarity, unlock time, progress |
| Metadata editor | Redesign | P1 | Manual title, description, IDs, genres, developers, publishers |
| Regional titles | Port | P1 | Alternate names shown, primary title chosen |
| Game insights refresh | Port | P1 | Explicit refresh; stale cache remains usable |
| Copy launch details | Port | P0 | Clipboard text, no secrets |
| Open in launcher | Port | P0 | Source-specific command, argv only |

---

## 4. Launch

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Steam launch | Port | P0 | `steam -applaunch <id>` or `steam://rungameid/<id>` with validation |
| Heroic launch | Port | P0 | Heroic CLI/URL; Flatpak form |
| Lutris launch | Port | P0 | `lutris lutris:rungame/<id>` argv |
| GOG direct launch | Port | P0 | Native executable or `umu-run` |
| RetroArch launch | Port | P0 | `retroarch -L <core> <rom>` |
| Emulator launch plans | Port | P0/P1 | Per-source argv builders; no shell |
| Manual game launch | Port | P0 | User command parsed without a shell; clearly labelled |
| Flatpak launch | Port | P0 | `flatpak run <validated id> -- <args>` |
| Launch setup overrides | Port | P1 | Core selection, flatpak/native, path, reset |
| Launch inspector | Port | P0 | Content availability, missing emulator/core, actionable error |
| Launch hold | Adapter | P2 | Compositor-specific; off by default |
| Warm Steam | Adapter | P2 | Start Steam minimized where support exists |
| Close after launch | Port | P0 | Setting; hide window, keep session/recorder |
| Launch feedback / starting screen | Redesign | P1 | Notice or overlay with artwork and listed held windows |
| Copy command | Port | P0 | Forums/support; no secret |

**Security invariant:** every launch path builds an argv vector. No `system()`, `popen()`,
`bash -c`, `sh -c` or string-concatenated command exists in the code base. This is enforced
by review, grep-based CI and tests.

---

## 5. Play sessions and statistics

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Separate recorder daemon | Port | P0 | `lgl-games-library-sessiond` |
| Process matching profiles | Port | P0 | `/proc` + emulator profiles |
| Session open/extend/close | Port | P0 | Monotonic time; 5 s poll |
| Durable journal / replay | Port | P0 | Refused writes survive restart |
| Ownership lock | Port | P0 | One recorder per library |
| Pause on unfocus | Adapter | P1 | Needs compositor focus data; off by default |
| Compositor window-title attribution | Adapter | P1 | KineticWE/KWin window list; wlroots foreign-toplevel; no-op elsewhere |
| Dolphin heartbeat record | Port | P1 | Read-only, bounded, no guessing |
| PCSX2 log attribution | Port | P1 | Bounded append tracking |
| Imported emulator totals | Port | P1 | Baselines and watermarks |
| Statistics page | Redesign | P0 (basic), P1 (full) | Period, totals, per source/console, top games |
| Year in Review | Redesign | P1 | Cairo-rendered PNG; local only |
| Delete history | Port | P1 | Confirmation; recorder-safe |
| Discord Rich Presence | Port | P1 | Opt-in; local IPC; no secrets |

---

## 6. Backup, saves and streaming

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Library backup export | Port | P1 | `.omakade-backup` v2; atomic; 0600 |
| Library backup preview/restore | Port | P1 | Validate, merge/replace, queue for exclusive startup |
| Backup recovery | Port | P1 | Interrupted restore tests |
| Save protection (RetroArch) | Port | P1 | Layouts, snapshots, restore journal |
| Save protection (Dolphin, PCSX2, Cemu, shadPS4, Ryujinx, melonDS, native) | Port | P1/P2 | One adapter per emulator; explicit availability |
| Save coverage UI | Redesign | P1 | Entries, cleanup, storage usage |
| Portable save layouts | Port | P1 | JSON config, validated |
| Save restore recovery | Port | P1 | Retry UI; refuse while emulator runs |
| Sunshine/Moonlight export | Port | P2 | `apps.json` markers; restart needed |
| ROMs/saves in library backup | Excluded | — | Upstream behaviour preserved |

---

## 7. Controller, Game Mode and guide

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| SDL 3 controller input | Port | P0 (polling), P1 (navigation) | Keep SDL 3; virtual gamepad in tests |
| Controller focus engine | Redesign | P1 | Rendered-position navigation, held repeat, reconnects |
| Fullscreen Game Mode on current display | Redesign | P1 | Same window, fullscreen, controller layout |
| Chosen display and sound output | Adapter | P2 | KineticWE `kde-output-management-v2` / KWin D-Bus + PipeWire |
| Notifications silencing | Adapter | P2 | Noctalia `notification-dnd-set/status` IPC |
| Hold launcher windows | Adapter | P2 | KineticWE KWin scripting; state journal |
| Restore desktop state | Adapter | P2 | KineticWE KWin scripting; state journal |
| Guide button (evdev) | Port | P2 | Read-only; permissions documented |
| In-game guide overlay | Not available | — | KineticWE does not provide layer-shell; no GTK overlay is possible |
| Guide actions (resume, stop, clip) | Not available | — | No guide overlay on KineticWE; actions must not pretend to open one |
| Game Mode keyboard shortcut | Adapter | P2 | KGlobalAccel or GlobalShortcuts portal |
| Couch navigation contract tests | Redesign | P1 | GTK + SDL virtual pad; every link checked |

### Platform expectations

| Capability | KineticWE + Noctalia (target) | Stock KDE | GNOME | wlroots |
| --- | --- | --- | --- | --- |
| Fullscreen window | yes | yes | yes | yes |
| Layer-shell overlay | no | yes (Plasma) | no | yes |
| Guide overlay | no (no layer-shell) | Plasma yes | no | yes |
| Window move/workspace control | KWin D-Bus + scripting | KWin scripting | no public API | foreign-toplevel |
| Output enable/disable | `kde-output-management-v2` | KScreen/KWin | no public API | wlr-randr |
| Audio routing | PipeWire | PipeWire | PipeWire | PipeWire |
| Global shortcut | KGlobalAccel + portal | portal/KGlobalAccel | portal if implemented | compositor binding |
| Notifications DND | Noctalia IPC | desktop-specific | no public API | compositor-specific |

The UI must report this table in Diagnostics and disable unsupported controls with a reason.
The target column is the acceptance environment; the others are compatibility information.

---

## 8. Highlights and clips

| Feature | Decision | Phase | Notes |
| --- | --- | --- | --- |
| Replay buffer | Defer | P3 | No Fedora `gpu-screen-recorder`; optional external adapter |
| Save last N seconds | Defer | P3 | External recorder or new PipeWire/GStreamer design |
| Achievement clips | Defer | P3 | Depends on replay buffer |
| Clip library, stills, trash | Defer | P3 | Extend if replay buffer is built |
| `--clip` command | Defer | P3 | Should fail clearly when unavailable |
| Capture button | Defer | P3 | Guide-button helper extension |

A future recorder design must use the ScreenCast portal, explicit user consent, bounded
output, no audio capture without a setting, and no always-on recording by default. It is a
separate security review.

---

## 9. Theme, shell and desktop integration

| Upstream feature | Decision | Replacement |
| --- | --- | --- |
| Omarchy theme colours/fonts | Drop | Noctalia-generated GTK theme + libadwaita named colours + ThemeWatcher |
| Hyprland gaps/rounding | Drop | GTK standard radii and spacing |
| Omarchy shell guide plugin | Drop | KineticWE does not provide layer-shell; no overlay replacement |
| `omarchy-shell` DND | Replace | Noctalia `notification-dnd-set/status` IPC |
| Hyprland Lua bindings | Drop | KGlobalAccel / Global Shortcuts portal + Kinetic Settings documentation |
| Noctalia shell surfaces | Do not embed | Normal GTK client; desktop file + icon |
| Noctalia plugins | Do not load | Plugins are trusted Luau; out of scope |
| Noctalia theme config | Do not read/write | Consume the generated GTK theme only |
| Idle inhibit | Port | `Gtk::Application::inhibit` / KineticWE idle-inhibit |
| Single instance | Port | `Gio::SocketService` |
| Systemd user service | Port | Standard user unit |
| Desktop actions | Port | Game Mode action in `.desktop` |
| MIME/file opening | Not needed | Library does not open files from the file manager |

---

## 10. Tests and fixtures to preserve

The upstream test suite is valuable as a behaviour specification. Port the intent, not the
Qt Test code:

| Upstream test | Port target |
| --- | --- |
| `CoreTests.cpp` (15,508 lines) | Split into per-module `g_test` executables |
| `GameModeTests.cpp` (4,257) | GTK focus contract + compositor adapter tests |
| `ReleaseHardeningTests.cpp` (1,773) | Security/limits tests |
| `InGameGuideTests.cpp` (1,118) | Not ported; guide unavailable on the target |
| `BackupRecoveryTests.cpp` (956) | Backup/recovery tests |
| `HighlightTests.cpp` (796) | Deferred with the feature; keep protocol tests |
| `SaveBackupsTests.cpp` (567) | Save layout/snapshot tests |
| Python launcher/catalogue tests | C++ `g_test` or CTest fixture scripts |
| Fixtures under `tests/fixtures/` | Copy only non-personal, redistributable fixtures |

Never copy real ROMs, keys, saves, credentials or personal library data into this repository.

---

## 11. Parity summary

| Area | P0 | P1 | P2 | P3 | Drop |
| --- | --- | --- | --- | --- | --- |
| Discovery | Steam, Heroic, Lutris, GOG, RetroArch, ROM folders, manual | remaining emulators, ports, Faugus | Battle.net, Xodus, RomM | — | AUR assumptions |
| Library UI | grid, search, filters, sort, details | organisation, editors, consoles, links | — | — | QML layout |
| Launch | main sources, Flatpak, manual | setups, feedback | hold/warm Steam | — | shell commands |
| Tracking | recorder, totals, basic stats | history, charts, Year in Review, Discord | focus adapters | — | Hyprland-only |
| Backup/saves | — | full | extra emulators | — | — |
| Game Mode | — | current-display fullscreen | dedicated display, hold, DND | shortcuts | Omarchy guide |
| Highlights | — | — | — | ScreenCast/external recorder | built-in assumption |
| Theme | GTK shell | — | — | — | Omarchy |
