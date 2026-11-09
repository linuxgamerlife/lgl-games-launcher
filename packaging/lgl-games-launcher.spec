Name:           lgl-games-launcher
Version:        0.3.0
Release:        1%{?dist}
Summary:        Steam and Faugus game library and launcher for Fedora

License:        GPL-3.0-or-later
URL:            https://github.com/linuxgamerlife/lgl-games-launcher
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
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(libsoup-3.0)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(libsecret-1)
BuildRequires:  pkgconfig(webkitgtk-6.0)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(libarchive)
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

Requires:       hicolor-icon-theme

%description
LGL Games Launcher reads Steam and Faugus games from local library data,
presents them in a GTK library, and launches them through the original
launcher. Faugus entries include the Ubisoft Connect, EA App and Epic Games
launchers it manages, plus the games those clients installed. Epic sign-in can
also list owned Epic games, installed or not. Optional Ubisoft Connect sign-in
does the same for Ubisoft games.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/com.linuxgamerlife.%{name}.desktop
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/com.linuxgamerlife.%{name}.metainfo.xml

%files
%license LICENCE.md LICENSE
%doc README.md CHANGELOG.md HELP.md
%{_bindir}/%{name}
%{_datadir}/applications/com.linuxgamerlife.%{name}.desktop
%{_datadir}/metainfo/com.linuxgamerlife.%{name}.metainfo.xml
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png

%changelog
* Mon Nov 09 2026 LinuxGamerLife - 0.3.0-1
- Add optional Ubisoft Connect sign-in and owned-library sync
- Add Ubisoft cover art and uplay:// launch/install handling

* Mon Nov 09 2026 LinuxGamerLife - 0.2.0-1
- Add Faugus discovery, including Ubisoft Connect, EA App and Epic Games entries
- Add store game discovery, art and direct launch for Ubisoft Connect, EA App and Epic Games
- Add optional Epic sign-in and owned-library sync
- Add a source filter for Steam, Faugus, Epic, EA and Ubisoft
- Launch Faugus entries with faugus-launcher and open Faugus from the details page

* Mon Nov 09 2026 LinuxGamerLife - 0.0.1-2
- Ship the full GPL-3.0 licence text in LICENSE as an RPM licence file

* Mon Nov 09 2026 LinuxGamerLife - 0.0.1-1
- Initial Steam-only MVP: discovery, library browsing, details and launch
