# 14 — Platform support

The design target is **Fedora with KineticWE (KWin-based compositor) and the Noctalia shell**.
Other Wayland sessions are compatibility targets. They must never be used to justify a design
that is wrong for the target, and the UI must tell the user what is missing.

---

## 1. Reference platform

| Component | Reference | Notes |
| --- | --- | --- |
| OS | Fedora 44 x86_64 | Primary build and acceptance |
| Session | KineticWE Wayland | KWin fork, tiling, KDE-compatible D-Bus names |
| Shell | Noctalia (KineticWE fork, `noctalia-kwe`) | Bars, notifications, DND, launcher, GTK theme generation |
| Portal | `xdg-desktop-portal-kwe` | ScreenCast, Screenshot, GlobalShortcuts, file dialogs |
| Audio | PipeWire + WirePlumber | `wpctl` / `pactl` compatibility |
| Toolkit | GTK 4.22 + gtkmm 4.22 | libadwaita 1.9 only for init/style manager |
| Display server | Wayland only | X11 is not a Game Mode target |

Record the exact KineticWE commit, Noctalia version, portal version, GTK version and kernel in
every compatibility report. A beta compositor changes; do not claim behaviour without a
version.

---

## 2. Capability matrix

| Capability | KineticWE + Noctalia | Stock KDE Plasma | GNOME | wlroots (Sway/Hyprland) |
| --- | --- | --- | --- | --- |
| GTK app / library | yes | yes | yes | yes |
| Launch / tracking | yes | yes | yes | yes |
| System title bar | KWin decoration | KWin | Mutter | compositor |
| Layer-shell protocol | no (not provided to clients) | yes | no | yes |
| `gtk4-layer-shell` overlay | no | yes | no | yes |
| KWin D-Bus / scripting | yes | yes | no | no |
| Output enumeration/control | `kde-output-management-v2` | KScreen/KWin | no public API | wlr-randr |
| Virtual desktops | `org.kde.KWin.VirtualDesktopManager` | KWin | no public API | ext-workspace/foreign-toplevel |
| Global shortcut | embedded KGlobalAccel + portal | KGlobalAccel/portal | portal if implemented | compositor binding |
| ScreenCast | KineticWE portal | KDE portal | GNOME portal | wlroots portal |
| Notification daemon | Noctalia | Plasma | GNOME Shell | Noctalia/other |
| DND control | Noctalia IPC | desktop-specific | no public API | Noctalia/other |
| GTK theme generation | Noctalia templates | Plasma | GNOME | Noctalia/other |

“Likely” is not a promise. Probe at runtime and record the result in Diagnostics.

---

## 3. Feature degradation

| Feature | Without the capability |
| --- | --- |
| Library, search, details, launch, tracking | Always available |
| Game Mode fullscreen on current display | Available; no window/output management |
| Dedicated display / audio routing | Requires KineticWE output D-Bus + PipeWire; otherwise disabled with a reason |
| Hold launcher windows / restore desktop | Requires KWin scripting; otherwise disabled |
| DND silencing | Requires Noctalia IPC; otherwise leave notifications alone |
| Guide overlay | Not available: KineticWE does not provide layer-shell |
| Global shortcut | KGlobalAccel/portal; otherwise user binds the CLI manually |
| Highlights | Only if a recorder is available; otherwise `--clip` fails clearly |

The application must never pretend a feature ran. A disabled control has a one-line reason and
a Diagnostics entry.

---

## 4. KineticWE integration details

### 4.1 What to use

- `org.kde.KWin` (owner: the compositor process) for workspace/desktop and general operations.
- `org.kde.KWin.VirtualDesktopManager` for virtual desktops.
- `org.kde.kwin.Scripting` for a bundled, allow-listed KWin script that moves/holds windows.
- `kde-output-management-v2` / output D-Bus for outputs.
- `org.kde.kglobalaccel` for global shortcuts (embedded in KineticWE).
- `org.kde.KWin.ScreenCast` / Screenshot and `xdg-desktop-portal-kwe` for capture.
- KineticWE idle-inhibit/DPMS for screen wake.

### 4.2 What not to do

- Do not call `hyprctl`, read Hyprland config, or write `~/.config/hypr/`.
- Do not assume `org.kde.KWin` methods from a different KWin release exist; probe.
- Do not inject a KWin script from a database, network or user string.
- Do not disable the only output without confirmation.
- Do not leave DND on, a window held, or an output disabled after a crash.
- Do not use Activities (removed in KineticWE 2.0).

### 4.3 No layer-shell, no guide overlay

KineticWE does not provide `zwlr_layer_shell_v1` to clients. The source tree contains an
implementation, but it is not a supported client surface. Therefore:

- `gtk4-layer-shell` is not used and not packaged.
- A GTK overlay above a fullscreen game cannot be created on the target.
- The in-game guide is unavailable and the UI says so.
- A normal fullscreen guide window is not a substitute; it can steal input from the game.
- A future guide requires KineticWE to expose a supported overlay surface; that is an upstream
  change and a new design/security review.

The obsolete spike plan has been replaced by this decision. Do not re-add a “try it anyway”
path: it would ship a known-unsupported dependency.

---

## 5. Noctalia integration details

- Noctalia owns notifications. Use `GNotification`; do not implement a notification daemon.
- DND: `noctalia-kwe msg notification-dnd-status`, `... notification-dnd-set on|off`,
  `... notification-dnd-toggle`. Save the previous state, set it, restore it. Parse a boolean,
  cap output, timeout.
- Theme: Noctalia’s palette/template system writes GTK 3/4 theme and color scheme. Follow GTK
  named colours; do not read or write Noctalia’s config.
- Launcher: a normal `.desktop` file; Noctalia scans XDG entries.
- Plugins: do **not** load or depend on Noctalia Luau plugins. They are trusted shell code and
  outside this app’s trust boundary.
- IPC: the socket is per-user; invoke with argv, fixed commands, bounded output. Never
  construct a command from a game title/path.

If Noctalia is absent, the app still runs. Notifications use the freedesktop daemon if one
exists; DND controls are disabled.

---

## 6. Fallback sessions

### 6.1 Stock KDE Plasma

The KWin APIs are similar; most Game Mode features should work. Test and report separately.
Do not assume the same portal/DND integration; Noctalia IPC is absent.

### 6.2 GNOME

The library, launch and tracking work. Layer-shell is absent, and there is no public window
management API. Game Mode is a fullscreen window on the current display; dedicated display,
hold and overlay are unavailable. The UI says so.
### 6.3 wlroots (Sway/Hyprland)

Layer-shell and foreign-toplevel exist; a future adapter can support Game Mode. Do not make it
a 1.0 requirement. Hyprland-specific `hyprctl` code from upstream is not ported.

### 6.4 X11

The GTK app can run under X11, but Game Mode/output/window control and layer-shell are not
supported. X11 is not an acceptance target.

### 6.5 Nested/gamescope

Test fullscreen Game Mode under gamescope. The guide overlay is unavailable there for the same
reason as on KineticWE: no supported layer-shell surface. Document the result; do not claim
overlay support.

---

## 7. Launcher availability on Fedora

| Source | Typical Fedora install | Discovery |
| --- | --- | --- |
| Steam | RPM Fusion or Flatpak | Native roots and Flatpak exports |
| Lutris | Fedora official | Native config + Flatpak |
| Heroic | Flathub | Flatpak exports + config |
| GOG direct | user files | Standard folders + extra paths |
| RetroArch | Fedora official or Flatpak | Config/playlists + Flatpak |
| Emulators | Flatpak, COPR or third-party RPM | Generic roots + Flatpak IDs |
| Sunshine | Flatpak/third-party | Config detection |
| Discord | Flatpak/third-party | Runtime socket candidates |
| Faugus | not a Fedora package | Detect if present; no promise |
| Xodus/Battle.net | Wine/Proton/Bottles | Prefix discovery only |

A missing source is not an error. Do not require a launcher to install the library app.

---

## 8. Hardware

- GPU: any GPU that runs the desktop; artwork decode is CPU/GTK, recording is P3.
- Controllers: SDL 3 supports Xbox, PlayStation, Nintendo and generic HID; test one of each.
- Displays: 720p to 4K, 1x/1.25x/1.5x/2x scale, multi-monitor and hotplug.
- Audio: PipeWire default; `pactl` compatibility for JSON listing.
- ARM64: build in CI; hardware acceptance is separate and not claimed at 1.0.

---

## 9. Compatibility reports

Every report includes:

- Fedora version, kernel, GPU/driver;
- KineticWE commit and Noctalia version;
- GTK/gtkmm and portal versions;
- session type and desktop;
- source launcher and game/emulator versions;
- what was tested, expected/actual, logs (redacted);
- whether the feature is native, degraded or unavailable.

Do not turn a single report into a general compatibility claim.
