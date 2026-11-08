Name:           lgl-games-launcher
Version:        0.1.0
Release:        1%{?dist}
Summary:        Steam library and launcher for Fedora

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
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

Requires:       hicolor-icon-theme

%description
LGL Games Launcher reads installed Steam games from local manifests, presents
them in a GTK library, and launches them through Steam or the Flatpak Steam
client. This is the Steam-only MVP.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/%{name}.metainfo.xml

%files
%license LICENCE.md
%doc README.md CHANGELOG.md HELP.md
%{_bindir}/%{name}
%{_datadir}/applications/com.linuxgamerlife.%{name}.desktop
%{_datadir}/metainfo/com.linuxgamerlife.%{name}.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg

%changelog
* Sun Nov 08 2026 LinuxGamerLife - 0.1.0-1
- Initial Steam-only MVP: discovery, library browsing, details and launch
