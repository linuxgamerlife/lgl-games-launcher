# 18 — Standards compliance matrix

This document maps the LGL standards to the project. The GTK standard is the primary
user-facing standard; the C++/Qt standard’s Qt-specific mechanisms are mapped to GLib/gtkmm
equivalents in [16-Coding standards](16-CODING-STANDARDS.md). Security is non-negotiable.

Status meanings: **Planned** (design meets it), **Deviation** (documented difference),
**N/A** (not applicable).

---

## 1. GTK Desktop Application Standard

| Clause | Requirement | Status | Where |
| --- | --- | --- | --- |
| Stack | gtkmm 4.20+, libadwaita 1.6+ C API only | Planned | [11-Build](11-BUILD.md) |
| Language/build | C++20, CMake 3.20+, GResource | Planned | [11-Build](11-BUILD.md) |
| Tests | GLib `g_test` via CTest | Planned | [12-Testing](12-TESTING.md) |
| Packaging | RPM on COPR | Planned | [13-Packaging](13-PACKAGING.md) |
| App ID | `com.linuxgamerlife.lgl-games-library` | Planned | [05-UI](05-UI-SPEC.md) |
| Binary/icon/config | `lgl-games-library` | Planned | [05-UI](05-UI-SPEC.md) |
| Display/wordmark | `LGL Games Library` / `LGL GAMES LIBRARY` | Planned | [05-UI](05-UI-SPEC.md) |
| GResource prefix | `/com/linuxgamerlife/gameslibrary/` | Planned | [03-Architecture](03-ARCHITECTURE.md) |
| Namespace/CSS prefix | `gameslibrary` / `gl-`, `@gl_` | Planned | [05-UI](05-UI-SPEC.md) |
| Project layout | root GTK app, `src/core` widget-free, `src/ui` | Planned | [03-Architecture](03-ARCHITECTURE.md) |
| Legacy sources | `legacy/qt/` not built | Planned | [15-Migration](15-MIGRATION.md) |
| Lifecycle | one Gtk::Application, window in `main`, standard startup order | Planned | [05-UI](05-UI-SPEC.md) |
| Quit action | Ctrl+Q, hide then quit | Planned | [05-UI](05-UI-SPEC.md) |
| Theming | named colours, `gl_` derived, no hardcoded palette | Planned | [05-UI](05-UI-SPEC.md) |
| Status colours | fixed light/dark, `.dark` class | Planned | [05-UI](05-UI-SPEC.md) |
| ThemeWatcher | live reload, 200 ms debounce, no-portal fallback, mono, `.dark`, signal | Planned | [05-UI](05-UI-SPEC.md) |
| Window radius | 0 | Planned | [05-UI](05-UI-SPEC.md) |
| Components/spacing/radii | standard classes and values | Planned | [05-UI](05-UI-SPEC.md) |
| Notices | Revealer, 5/10 s, no target | Planned | [05-UI](05-UI-SPEC.md) |
| Title bar modes | app/system, Ctrl+Shift+M, menu, persisted | Planned | [05-UI](05-UI-SPEC.md) |
| Header bar | logo, actions, primary, menu, tooltips | Planned | [05-UI](05-UI-SPEC.md) |
| Main menu | standard sections, Alt, F10 | Planned | [05-UI](05-UI-SPEC.md) |
| Shortcuts | standard set, accelerators | Planned | [05-UI](05-UI-SPEC.md) |
| About | standard fields, GPL-3.0 | Planned | [05-UI](05-UI-SPEC.md) |
| Dialogs | FileDialog/AlertDialog/custom, notices | Planned | [05-UI](05-UI-SPEC.md) |
| Help | `HELP.md` in GResource, F1 | Planned | [05-UI](05-UI-SPEC.md) |
| Settings | `settings.ini`, typed, 0700, atomic | Planned | [04-Data model](04-DATA-MODEL.md) |
| Background work | no main-loop blocking, dispatcher, generation | Planned | [03-Architecture](03-ARCHITECTURE.md) |
| External tools | Gio::Subprocess, argv, timeout | Planned | [09-Security](09-SECURITY.md) |
| Warnings/sanitizers | -Werror, ASan/UBSan option | Planned | [11-Build](11-BUILD.md) |
| Tests | settings_test, theme_test, skip 77 | Planned | [12-Testing](12-TESTING.md) |
| Snapshot | `LGL_GAMES_LIBRARY_GTK_SNAPSHOT` | Planned | [12-Testing](12-TESTING.md) |
| Packaging | spec, COPR, Makefile, icons, metainfo | Planned | [13-Packaging](13-PACKAGING.md) |
| Compliance record | this document | Planned | — |

**Deviation D-01:** the app’s data directory (`$XDG_DATA_HOME/lgl-games-library/`) is required
for the library database, artwork and save sets. The standard’s settings/history table does not
name a data directory; this is an extension, not a conflict. Documented in
[04-Data model](04-DATA-MODEL.md).

**Deviation D-02:** Noctalia generates the GTK theme from its palette. The app still follows
named GTK colours and never ships a palette; ThemeWatcher is unchanged. This is a theme source,
not a deviation from theme-following.

**Deviation D-03:** the in-game guide overlay is not available because KineticWE does not
provide layer-shell. The standard does not require an overlay; the feature is dropped on the
target, not hidden behind a broken implementation.

---

## 2. C++ / Qt Development Standard

| Clause | GTK equivalent | Status |
| --- | --- | --- |
| C++20 preferred, C++17 min | C++20 | Planned |
| RAII, stack allocation, smart pointers | same | Planned |
| No owning raw pointers | same | Planned |
| QObject parent ownership | gtkmm container ownership / Glib::RefPtr | Planned |
| No C-style casts | same | Planned |
| Check errors, result model | `std::expected`/Result, Glib::Error at boundary | Planned |
| Input validation | boundary validators, `SEC-*` | Planned |
| No UI-thread blocking | worker + dispatcher | Planned |
| File path safety | `SEC-PATH-*` | Planned |
| TLS/network safety | libsoup 3, `SEC-NET-*` | Planned |
| No hardcoded secrets | Secret Service | Planned |
| Logging redaction | `SEC-LOG-*` | Planned |
| `-Wall -Wextra -Wpedantic -Werror`, GCC/Clang | same | Planned |
| clang-tidy/cppcheck | same | Planned |
| clang-format | same | Planned |
| ASan/UBSan | same | Planned |
| Test coverage, regression tests | g_test, CTest | Planned |
| Fuzzing | archive/parser targets | Planned |
| Architecture: UI vs logic | core/ui split | Planned |
| Ownership/lifetime/thread/failure documented | [16-Coding](16-CODING-STANDARDS.md) | Planned |

The Qt-specific clauses (QObject threading, Qt SQL, Qt networking, QML tests) are **N/A** and
are replaced by the GTK equivalents in [16-Coding standards](16-CODING-STANDARDS.md).

---

## 3. Privileged Operations Standard

| Clause | Status |
| --- | --- |
| GUI never root | Planned |
| Privileged functionality isolated | N/A — none |
| Allow-listed operations | N/A — no privileged helper |
| Avoid shell | Planned (all execution) |
| Input validation | Planned |
| Fail safe | Planned |
| Logging | Planned |
| Idempotent | Planned where applicable |

If any privileged operation is proposed, this section becomes **blocking** and the design must
be redone under the standard.

---

## 4. Secure Design Review Standard

| Clause | Status | Where |
| --- | --- | --- |
| Feature description | Planned per feature | [09-Security](09-SECURITY.md) §6 |
| Assets affected | Planned | §2 |
| Trust boundaries | Planned | §3 |
| Malicious input considered | Planned | §6–7 |
| Command failure | Planned | §6 |
| Misuse/repetition | Planned | §6–7 |
| Least privilege | Planned | §5.1 |
| Allowlists | Planned | §5 |
| Input validation | Planned | §5 |
| Review before development | Planned | Phase 0 gate |
| Compliance checklist | Planned | §10 |

---

## 5. Release Integrity Standard

| Clause | Status | Where |
| --- | --- | --- |
| Build in controlled CI | Planned | [13-Packaging](13-PACKAGING.md) |
| Reproducible/documented | Planned | §7–9 |
| Signing | Planned | §9 |
| Checksums | Planned | §9 |
| Dependency control | Planned | §9, [11-Build](11-BUILD.md) |
| Vulnerability response | Planned | [09-Security](09-SECURITY.md) §9 |
| Release notes | Planned | [08-Roadmap](08-ROADMAP.md) |

---

## 6. AI-Assisted Development Safety Standard

| Clause | Status | Where |
| --- | --- | --- |
| AI output untrusted | Planned | [12-Testing](12-TESTING.md) |
| Static analysis/CodeQL | Planned | §7 |
| Warnings as errors | Planned | [11-Build](11-BUILD.md) |
| Sanitizers | Planned | [11-Build](11-BUILD.md) |
| Dependency scanning | Planned | §7 |
| Automated tests | Planned | [12-Testing](12-TESTING.md) |
| High/Critical resolved | Planned | release gate |
| No bypassing verification | Planned | [07-Implementation](07-IMPLEMENTATION-PLAN.md) |

---

## 7. Qt to GTK Repository Migration Standard

| Clause | Status | Where |
| --- | --- | --- |
| Root GTK layout | Planned | [15-Migration](15-MIGRATION.md) |
| No `gtk/` wrapper | Planned | §1 |
| `legacy/qt/` archive | Planned | §2 |
| Root CMake builds GTK | Planned | §3 |
| Source archive excludes legacy | Planned | [13-Packaging](13-PACKAGING.md) |
| Packaging/CI root paths | Planned | §7 |
| Docs updated | Planned | §6 |
| Validation table | Planned | §8 |

---

## 8. Secure Web Application Development Standard

This is a desktop client, not a web application. The relevant principles are adopted:

- TLS and certificate validation;
- input validation and injection prevention;
- no embedded remote code;
- bounded responses and timeouts;
- dependency and supply-chain security;
- no credentials in URLs/logs.

The browser-specific clauses (CSP, cookies, CSRF, DOM) are **N/A**.

---

## 9. Known deviations and open items

| ID | Item | Reason | Resolution |
| --- | --- | --- | --- |
| D-01 | Extra XDG data directory | Library DB/artwork/saves need it | Documented; standard extension |
| D-02 | Noctalia-generated theme | Target desktop owns the palette | App follows GTK named colours |
| D-03 | Guide overlay unavailable | KineticWE does not provide layer-shell | Drop; revisit only if compositor adds support |
| D-04 | Flatpak deferred | Host access defeats sandbox honesty | Revisit post-1.0 |
| D-05 | Highlights deferred | No Fedora recorder | Separate design/review |
| D-06 | No TSan on all tests | GTK/GLib interactions | TSan on recorder/worker tests |

No deviation from the security requirements is permitted. If a future change needs one, it must
be recorded here with an explicit risk acceptance, an owner and a removal plan.
