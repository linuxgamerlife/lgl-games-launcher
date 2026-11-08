# 13 — Packaging and release integrity

Target: Fedora 44+, RPM and COPR. Flatpak is explicitly deferred and is not a 1.0 deliverable.
The packaging follows the LGL GTK standard and the
[Release Integrity Standard](../../docs/release-integrity-standard.md).

---

## 1. Identity

| Item | Value |
| --- | --- |
| Package | `lgl-games-library` |
| Binary | `/usr/bin/lgl-games-library` |
| Application ID | `com.linuxgamerlife.lgl-games-library` |
| Desktop file | `com.linuxgamerlife.lgl-games-library.desktop` |
| Metainfo | `com.linuxgamerlife.lgl-games-library.metainfo.xml` |
| Icon | `lgl-games-library` |
| Config | `~/.config/lgl-games-library/settings.ini` |
| Data | `~/.local/share/lgl-games-library/` |
| Cache | `~/.cache/lgl-games-library/` |
| User units | `lgl-games-library-sessiond.service`, optional guide-button unit |
| Licence | `GPL-3.0-or-later` |
| Upstream | independent GTK port; Omakade attribution preserved |

The app id must not reuse `io.github.tsouth89.Omakade`. The port is a different application
with different data paths; reusing the id would make software centres and portals confuse the
two and could hand the wrong files to the wrong app.

---

## 2. Repository files

```text
CMakeLists.txt
Makefile
.copr/Makefile
packaging/
  lgl-games-library.spec
  com.linuxgamerlife.lgl-games-library.desktop
  com.linuxgamerlife.lgl-games-library.metainfo.xml
  icons/lgl-games-library.png                 # master artwork
  icons/48x48/lgl-games-library.png
  icons/64x64/lgl-games-library.png
  icons/128x128/lgl-games-library.png
  icons/256x256/lgl-games-library.png
  lgl-games-library-sessiond.service
  lgl-games-library-guide-button.service       # optional Home/Capture actions; no overlay
data/
  lgl-games-library.gresource.xml
  style.css
  icons/lgl-games-library.png                  # 256 px copy for header/About
  consoles/*.png
  native-ports.json
```

Icons are generated from the master with Lanczos resizing and stripped metadata. Keep the
master and the four installed sizes; the 256 px copy is bundled in the GResource.

---

## 3. RPM spec

The spec builds from the repository root with `%cmake -DBUILD_TESTING=ON`, runs `%ctest`, and
validates installed metadata. It does **not** build Qt or KF6.

```spec
Name:           lgl-games-library
Version:        0.1.0
Release:        1%{?dist}
Summary:        GTK game library for Fedora

License:        GPL-3.0-or-later
URL:            https://github.com/linuxgamerlife/lgl-games-library
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  cmake-rpm-macros
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(gtkmm-4.0) >= 4.20
BuildRequires:  pkgconfig(libadwaita-1) >= 1.6
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(libsoup-3.0)
BuildRequires:  pkgconfig(libsecret-1)
BuildRequires:  pkgconfig(libzip)
BuildRequires:  pkgconfig(libzstd)
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(sdl3)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(pango)
BuildRequires:  desktop-file-utils
BuildRequires:  appstream
BuildRequires:  systemd-rpm-macros
BuildRequires:  xorg-x11-server-Xvfb
BuildRequires:  dbus-run-session

Requires:       hicolor-icon-theme
Requires:       xdg-desktop-portal
Recommends:     kineticwe

%description
LGL Games Library brings installed Steam, Heroic, Lutris, GOG, emulator and
native-port games into one local GTK library for Fedora. It discovers games
read-only, delegates launching to the owning launcher or emulator, and can
record play sessions and protect emulator saves. KineticWE and Noctalia provide
the full Game Mode and notification experience; other Wayland sessions work with
reduced features.
```

Notes:

- `gtk4-layer-shell` is not a dependency: KineticWE does not provide layer-shell, so the guide
  overlay is unavailable and no overlay code is shipped.
- Do not require `kineticwe` hard; the app must start on another Wayland session and explain
  what is unavailable.
- Do not require Noctalia; it is part of the KineticWE session, not a library dependency.
- No `Requires: qt6-*`, `kf6-*`, `kwin`, or `noctalia`.

### 3.1 Scriptlets

```spec
%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

%check
%ctest --output-on-failure
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/%{name}.metainfo.xml
```

UI tests that need a display must skip cleanly in the build chroot (`SKIP_RETURN_CODE 77`).
Do not make the RPM build depend on a live KineticWE session.

### 3.2 Files

```spec
%files
%license COPYRIGHT LICENSE
%doc README.md CHANGELOG.md HELP.md docs/
%{_bindir}/lgl-games-library
%{_bindir}/lgl-games-library-sessiond
%{_bindir}/lgl-games-library-guide-button
%{_datadir}/applications/com.linuxgamerlife.lgl-games-library.desktop
%{_datadir}/metainfo/com.linuxgamerlife.lgl-games-library.metainfo.xml
%{_datadir}/icons/hicolor/48x48/apps/lgl-games-library.png
%{_datadir}/icons/hicolor/64x64/apps/lgl-games-library.png
%{_datadir}/icons/hicolor/128x128/apps/lgl-games-library.png
%{_datadir}/icons/hicolor/256x256/apps/lgl-games-library.png
%{_userunitdir}/lgl-games-library-sessiond.service
%{_userunitdir}/lgl-games-library-guide-button.service
%{_datadir}/lgl-games-library/
%{_datadir}/doc/lgl-games-library/
%changelog
```

Use `%{_userunitdir}` from `systemd-rpm-macros`; do not hardcode `/usr/lib/systemd/user`.
Ship the `graphical-session.target.wants` symlink through CMake install or the spec, not as a
generated file.

---

## 4. Desktop entry

```ini
[Desktop Entry]
Type=Application
Name=LGL Games Library
GenericName=Game Library
Comment=Your games from every launcher in one library
Exec=lgl-games-library
Icon=lgl-games-library
Terminal=false
Categories=Game;Player;
Keywords=Games;Launcher;Library;Steam;Heroic;Lutris;GOG;RetroArch;Emulator;
StartupNotify=true
StartupWMClass=com.linuxgamerlife.lgl-games-library
Actions=GameMode;

[Desktop Action GameMode]
Name=Game Mode
Exec=lgl-games-library --game-mode
```

- `StartupWMClass` must match the Wayland app id / WM class so the launcher groups the window.
- Add `MimeType` and `%F` only if the app opens files; it does not, so do not add them.
- Validate with `desktop-file-validate`.
- Noctalia’s launcher scans XDG entries; no plugin is required for the app to appear.

---

## 5. Metainfo

Required fields:

```xml
<component type="desktop-application">
  <id>com.linuxgamerlife.lgl-games-library</id>
  <name>LGL Games Library</name>
  <summary>Your games from every launcher in one library</summary>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>GPL-3.0-or-later</project_license>
  <developer id="com.linuxgamerlife">
    <name>LinuxGamerLife</name>
  </developer>
  <launchable type="desktop-id">com.linuxgamerlife.lgl-games-library.desktop</launchable>
  <content_rating type="oars-1.1" />
  <releases>...</releases>
</component>
```

- Add a `<release>` entry for every version, newest first.
- Add `<provides><id>com.linuxgamerlife.lgl-games-library</id></provides>` only if the id
  changes later; do not claim Omakade’s id.
- Screenshots must be local or HTTPS and must show the actual app.
- Validate with `appstreamcli validate --no-net`.

---

## 6. systemd user units

`lgl-games-library-sessiond.service`:

```ini
[Unit]
Description=LGL Games Library session recorder
Documentation=man:lgl-games-library(1)
PartOf=graphical-session.target
After=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/bin/lgl-games-library-sessiond
Restart=on-failure
RestartSec=5
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=%h/.local/share/lgl-games-library %h/.cache/lgl-games-library
Environment=GIO_USE_VFS=local

[Install]
WantedBy=graphical-session.target
```

- The daemon needs read access to `/proc`, launcher data, ROM folders and emulator configs.
  `ProtectHome=read-only` is required; do not use `ProtectHome=true`.
- It writes only to the app’s data/cache roots and the user-chosen video folder (P3). If it
  needs more, add a narrow `ReadWritePaths`.
- `NoNewPrivileges=true` and `PrivateTmp=true` are mandatory.
- The GUI does not use this unit; it starts the daemon on demand if the unit is not running,
  with a bounded timeout and a clear error.
- Validate with `systemd-analyze verify`.

The optional guide-button unit reads `/dev/input`; it needs the seat’s device permissions. Do
not add a world-readable udev rule. If permission is missing, the helper exits and the UI
explains.

---

## 7. Makefile and source archive

Root `Makefile`:

- `<name>-<version>.tar.gz` from the checked-out tree, excluding `.git`, `build*`, `result/`,
  `legacy/qt/`, test outputs and local caches.
- Include `src/`, `data/`, `tests/`, `packaging/`, `docs/`, `CMakeLists.txt`, `Makefile`,
  `README.md`, `CHANGELOG.md`, `HELP.md`, `COPYRIGHT`, `LICENSE`.
- Exclude `legacy/qt/` explicitly, per the migration standard.
- `.copr/Makefile` includes the root `Makefile` for COPR’s `make_srpm`.

```bash
make archive     # dist/lgl-games-library-<version>.tar.gz
make srpm        # result/lgl-games-library-<version>-1.fc44.src.rpm
```

The archive must reproduce the release commit; verify with `tar -tzf` and a clean build.

---

## 8. COPR

- Project: `linuxgamerlife/lgl-games-library` or the maintainer’s chosen COPR.
- Build for Fedora 44 x86_64 and aarch64.
- `make_srpm` method uses `.copr/Makefile`.
- COPR builds from the source archive, not a local binary.
- A failed build blocks the release.
- COPR’s repository metadata is signed by Fedora; the project still publishes its own
  checksums and signature for direct downloads.

---

## 9. Release integrity

Every release:

1. Build in CI/COPR from a tagged commit in a controlled Fedora 44 environment.
2. Generate a source archive and SRPM from the same commit.
3. Publish:
   - `lgl-games-library-<version>.tar.gz`
   - `lgl-games-library-<version>-1.fc44.src.rpm`
   - binary RPMs
   - `SHA256SUMS`
   - a detached signature (`minisign` or GPG) for `SHA256SUMS`
   - an SBOM (SPDX or CycloneDX)
   - release notes with known limitations
4. Verify the checksums and signature before publication.
5. Keep the previous release available for rollback.

Signing keys are stored offline or in a protected CI secret with restricted access. A release
must be reproducible enough that a clean COPR rebuild produces an equivalent package; document
any non-reproducible step.

---

## 10. Flatpak

Deferred. The app reads launcher data, `/proc`, arbitrary ROM/save paths and game libraries,
and launches host programs. A sandbox that is honest about that would need broad
`--filesystem=home`, `--filesystem=host`, process access and device access, which provides
little security. If Flatpak is attempted later:

- use `--filesystem=home` only with a clear explanation;
- use the ScreenCast and GlobalShortcuts portals, not direct compositor access;
- keep secrets in the portal Secret Service;
- document what is unavailable inside the sandbox;
- do not claim sandboxing as a security benefit.

A Flatpak is not a substitute for the RPM and is not part of the 1.0 release gate.

---

## 11. Versioning

- Semantic versioning for the app: 0.x during development, 1.0 at the stable gate.
- The version appears in:
  1. `packaging/lgl-games-library.spec` (`Version`, `Release`, `%changelog`);
  2. root `CMakeLists.txt` (`project(... VERSION ...)`);
  3. metainfo `<releases>`;
  4. `CHANGELOG.md` and the README current-release line.
- All four change together.
- No Git tag is required for COPR, but releases should be tagged for provenance.
- Do not encode “Omakade 1.18” as the app version; state compatibility in the notes instead.

---

## 12. Release checklist

- [ ] Version updated in all four places.
- [ ] `CHANGELOG.md` has a dated entry.
- [ ] Metainfo validates.
- [ ] Desktop file validates.
- [ ] Icons at 48/64/128/256, stripped, correct names.
- [ ] SRPM builds in a clean Fedora 44 chroot.
- [ ] Binary RPM installs, launches, uninstalls cleanly.
- [ ] systemd units verify and start.
- [ ] Tests pass; UI tests skip cleanly without a display.
- [ ] No Qt/KF6 dependency in the RPM.
- [ ] Source archive excludes `legacy/qt/`.
- [ ] SBOM and `SHA256SUMS` published.
- [ ] `SHA256SUMS` signed.
- [ ] Release notes list known limitations, especially the unavailable guide overlay and any
      optional recorder.
- [ ] Security sign-off.
