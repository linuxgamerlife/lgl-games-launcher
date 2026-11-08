# 20 — Glossary

| Term | Meaning |
| --- | --- |
| **ADR** | Architecture decision record; a dated, binding decision with alternatives and consequences. |
| **App ID** | `com.linuxgamerlife.lgl-games-library`; the desktop/application identity. |
| **Backup archive** | `.omakade-backup` ZIP containing personal library data and artwork, not ROMs/saves/credentials. |
| **Core** | `src/core`; widget-free domain logic, testable without a display. |
| **CTest** | CMake’s test runner; registers the `g_test` executables and UI/snapshot tests. |
| **DND** | Do Not Disturb; notification silencing. In the target session, Noctalia owns it. |
| **GResource** | GLib compiled resource bundle; holds CSS, icons, console art, catalogues, HELP. |
| **gtkmm** | C++ bindings for GTK 4; the application’s widget toolkit. |
| **KineticWE** | The target KWin-based Wayland compositor (KDE-compatible D-Bus names, tiling). |
| **KWin scripting** | The compositor’s JavaScript scripting interface used for allow-listed window operations. |
| **Layer shell** | `zwlr_layer_shell_v1`; a Wayland protocol for shell surfaces. KineticWE does not provide it to clients, so the app has no GTK overlay. |
| **LaunchPlan** | Typed program + argv + environment + working directory; the only way a process is started. |
| **Legacy Qt** | Omakade 1.18.0’s Qt/QML tree under `legacy/qt/`; read-only reference, not built. |
| **libadwaita** | GNOME’s style library; used here only for `adw_init` and `AdwStyleManager`, not for Adw widgets. |
| **Noctalia** | The target Wayland shell; notifications, DND, launcher, OSDs, GTK theme generation. |
| **Notice** | In-window, non-blocking result message (Revealer), not a dialog. |
| **Omakade** | The upstream Qt/QML application this project ports; GPL-3.0-or-later. |
| **P0/P1/P2/P3** | Release tiers defined in [02-Scope](02-SCOPE.md). |
| **Prepared statement** | A parameterised SQLite statement; the only way user data reaches SQL. |
| **Primary input** | The widget that receives focus when the user returns to the window. |
| **RPM/COPR** | Fedora package format and build service; the 1.0 packaging target. |
| **Save set** | A versioned, hashed snapshot of an emulator’s save files, with a restore journal. |
| **sessiond** | `lgl-games-library-sessiond`; the separate user service that records play sessions. |
| **Snapshot test** | Offscreen render of windows to PNG via `LGL_GAMES_LIBRARY_GTK_SNAPSHOT`. |
| **Source** | A launcher/emulator integration (Steam, Heroic, RetroArch, …). |
| **Spike** | A short, throwaway experiment to prove a platform assumption before scheduling work. |
| **TB** | Trust boundary in [09-Security](09-SECURITY.md). |
| **Untrusted input** | Any file, archive, network response, IPC message, argv, environment or database value not created by the app itself. |
| **XDG** | freedesktop base directory conventions (`XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`, `XDG_RUNTIME_DIR`). |
