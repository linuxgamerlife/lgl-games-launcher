# 05 — UI specification

This document defines the GTK look, behaviour and screen structure. It follows the
[LGL GTK Desktop Application Standard](../../docs/gtk-desktop-application-standard.md). The
standard is the authority; this document records how a game library applies it. Any
deviation is listed in [18-Standards compliance](18-STANDARDS-COMPLIANCE.md).

Status: draft for approval.

---

## 1. Identity

| Item | Value |
| --- | --- |
| Application ID | `com.linuxgamerlife.lgl-games-library` |
| Binary / icon / config | `lgl-games-library` |
| Display name | `LGL Games Library` |
| Wordmark | `LGL GAMES LIBRARY` |
| Desktop / metainfo | `com.linuxgamerlife.lgl-games-library.desktop`, `.metainfo.xml` |
| GResource prefix | `/com/linuxgamerlife/gameslibrary/` |
| C++ namespace | `gameslibrary` |
| CSS prefix | `gl-` for classes, `@gl_` for derived colours |
| CMake / env prefix | `LGL_GAMES_LIBRARY` |

Window title: `LGL Games Library`. Use `Glib::set_application_name()` and
`Gtk::Window::set_default_icon_name()` in `startup`.

---

## 2. Window shell

```text
┌──────────────────────────────────────────────────────────────────────┐
│ [icon] LGL GAMES LIBRARY   [Library] [Stats]        [Search]  [☰] [×] │  header
├──────────────────────────────────────────────────────────────────────┤
│  sidebar / filters        │  page content                             │
│  (sources, collections)   │                                           │
│                           │                                           │
│                           │                                           │
├──────────────────────────────────────────────────────────────────────┤
│                         [ notices revealer ]                         │
└──────────────────────────────────────────────────────────────────────┘
```

- The same `Gtk::HeaderBar` instance is used in both title bar modes.
- Logo: horizontal box, spacing 10, margin-end 12; icon 28 px, class `brand-icon`;
  wordmark label in capitals with class `wordmark`.
- The header bar's title widget is an empty `Gtk::Box` because the wordmark is the title.
- Start: page switching (`Library`, `Stats`) as flat text buttons, then contextual actions
  for the current page.
- End (packed right-to-left): main menu, then the primary action.
  - Library primary action: **Refresh** (`win.refresh`, `Ctrl+R`), tooltip
    `Rescan all sources (Ctrl+R)`.
  - Details primary action: **Play**, class `gl-primary suggested-action`.
- Every header button is bound to a `win.` action through `set_action_name()`.
- Every header button has a tooltip ending in its shortcut when one exists.
- An empty library still shows the logo, search and menu.

### Title bar modes

Two modes, switched at runtime and persisted as `[window] systemTitleBar`:

| Mode | Default | Who draws it | Header position |
| --- | --- | --- | --- |
| App title bar | yes | GTK | `set_titlebar(m_header)` |
| System title bar | no | compositor | first row inside `m_content` |

- Shortcut: **Ctrl+Shift+M**.
- Menu checkbox: **Use System Title Bar**.
- Apply the stored mode in the constructor, before realization.
- Runtime switching follows the standard's hold/hide/unrealize/move/re-attach/present/focus/
  release sequence. A single flicker is expected and documented.
- Primary input: the library grid or search field, whichever the user last used. Store an
  enum in `[window] primaryInput`.

---

## 3. Main menu

A `Gtk::MenuButton` with `open-menu-symbolic`, tooltip `Main menu`, `set_primary(true)`.

Sections, in order:

1. **Library**
   - Refresh (`Ctrl+R`)
   - Add ROM folder… (`Ctrl+Shift+O`)
   - Import Omakade data…
2. **View**
   - Use System Title Bar (checkbox, `Ctrl+Shift+M`)
   - Reduced Motion (checkbox, `Ctrl+M`)
3. **Help**
   - Help (`F1`)
   - Diagnostics… (source status, compositor capabilities, database path)
   - About LGL Games Library
4. **Quit** (`Ctrl+Q`)

Items that open a dialog or picker end with `…`. Menu actions use named `win.*`/`app.*`
actions so accelerators display and sensitivity follows state.

**Alt tap** opens and closes the main menu, with the standard capture-phase detector on both
the window and the menu popover. Closing with Alt returns focus to the primary input on idle.

---

## 4. Pages

### 4.1 Library

The default page. A responsive cover grid plus an optional list view.

```text
┌ filters ──────────────────────────────────────────────────────────────┐
│ Search…   [All sources ▾] [Installed ▾] [Sort: Title ▾]   [⚙ columns] │
├───────────────────────────────────────────────────────────────────────┤
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                    │
│  │cover │  │cover │  │cover │  │cover │  │cover │                    │
│  │title │  │title │  │title │  │title │  │title │                    │
│  └──────┘  └──────┘  └──────┘  └──────┘  └──────┘                    │
│                                                                       │
│  Console cards: [SNES] [N64] [PS2] [Switch] …                         │
└───────────────────────────────────────────────────────────────────────┘
```

- Grid: `Gtk::GridView` with `Gtk::SingleSelection`; min/max columns set from width and
  cover size. Cards are keyboard and controller focusable.
- List: `Gtk::ColumnView` with title, source, last played and playtime.
- Card: cover (4 px radius, `@gl_sunken` placeholder), title (2 lines, ellipsis), source
  chip, favourite mark, running indicator.
- Hover: `@gl_hover`. Selected: 22% accent mix. Focus: accent outline.
- Console cards appear before the main grid when console portals are enabled; a console card
  opens a filtered console page.
- Empty state: centred, wrapped `.muted` label: “No games found. Add a ROM folder or check
  your sources in Settings.”
- Search is always reachable (`Ctrl+F`), debounced 150 ms, case/diacritic-insensitive.
- Filter bar is a wrapped box; each filter is a menu button with a checkable list.
- Saved filters appear in the filter bar and the sidebar.

Sidebar (optional at narrow widths, collapsed into a menu):

- All games
- Favourites
- Recently played
- Installed / Ready to install
- Collections
- Tags
- Sources (with counts)
- Consoles

The sidebar uses `Gtk::ListBox` rows with a 12 px inset, not Adwaita widgets.

### 4.2 Game details

A page inside the main window (not a separate window) with a back button in the header.

```text
┌ [Back]  Title                                         [Play]  [☰] ┐
│ ┌──────────┐  Title                                              │
│ │  cover   │  Source · Platform · Last played · Playtime          │
│ │          │  [Favourite] [Hidden] [Collection ▾] [Tags…]        │
│ └──────────┘  Description / critic score / ProtonDB badge        │
│                                                                  │
│  Installations  [preferred] [change] [link…]                     │
│  Achievements   [progress list]                                  │
│  Save backups   [coverage] [snapshot] [restore…]                 │
│  Clips          [if highlights]                                  │
│  Metadata       [IGDB id] [refresh]                              │
└──────────────────────────────────────────────────────────────────┘
```

- Hero image or cover at the top; gradient overlay is drawn with CSS/`Gtk::Overlay`, not a
  fixed colour.
- Actions: Play (primary), Open in launcher, Copy launch details, Edit metadata, Artwork,
  Save backups, Link installations, Hide/Unhide, Delete history.
- Long sections use `Gtk::Expander` or a `Gtk::Stack` with a segmented switcher; all
  controls are reachable by keyboard and controller.
- Destructive actions use a confirmation dialog with verbs and a safe default.
- Missing data shows a muted “Not available” row, never a blank area.

### 4.3 Statistics

- Period switch: This year / All time.
- Summary cards: total playtime, games played, sessions, longest session, current streak.
- Per-source and per-console breakdown.
- Top games list with cover thumbnails.
- Month/week bar chart drawn with `Gtk::DrawingArea` and Cairo using theme colours.
- Year in Review: a preview card and **Save image…** (PNG) using Cairo at a fixed size
  (for example 1600×2000), with text and cover images. No external service.
- Empty state explains that play-session tracking can be enabled in Settings.

### 4.4 Settings

A modal or main-window page with a sidebar and pages. Use custom `Gtk::Box` cards, not
Adwaita preference widgets.

| Page | Contents |
| --- | --- |
| Sources | Detected sources, enable switches, rescan, ROM folders, console layout, diagnostics |
| Library | Sort, cover size, installed filter, console expansion, reduced motion, close after launch |
| Connections | Steam ID/API key, IGDB, SteamGridDB, RetroAchievements, RomM, ProtonDB; remove credentials |
| Controls | Controller navigation, Game Mode, home/capture button, pause on unfocus |
| Game Mode | Display, sound, notifications, warm Steam, hold launchers, shortcut (where supported) |
| Storage | Artwork cache limit, save protection, backup/restore, library database location, clear caches |
| Privacy | Network artwork, Discord presence, data summary, links to Privacy |
| About | Version, licence, credits, project page, support |

Every row has a title, a one-line subtitle, and a control. A changed setting saves
immediately and shows a notice only when the result is not obvious. A setting that cannot
work on this desktop is shown disabled with a one-line reason, not hidden silently.

### 4.5 Help

`HELP.md` at the repository root, bundled in the GResource, shown in a `Gtk::Window` with a
`Gtk::HeaderBar` titled `HELP` in the wordmark style. F1 and the main menu open it. It lists
every shortcut and every supported source.

### 4.6 Game Mode (P1/P2)

A fullscreen layout of the same library, not a separate application.

- Header: logo, `GAME MODE`, current time, controller hints.
- Content: large cards, 3–5 columns depending on resolution, focus ring and source chip.
- Footer: `A Select · B Back · Start Leave` (labels adapt to the controller). The upstream
  Home guide is unavailable on KineticWE; if a Home action is offered it must not claim to open
  an overlay.
- Focus follows the rendered layout: left/right in a row, up/down to the closest control in
  the adjacent row; wrapped rows are multiple rows. Hidden/disabled controls are skipped.
- Escape/B returns one level; Start/F11 opens the leave panel if a game is running.
- A game running indicator is visible; “Back to Game” returns to the game.
- Game Mode uses the same window and library model; entering does not reload the library.

---

## 5. Components and styling

Follow the standard's classes and measurements. Prefix app-specific classes with `gl-`.

| Element | Class | Rules |
| --- | --- | --- |
| Body | `.body` | 12 px padding |
| Card | `.gl-card` | `@gl_surface`, 1 px `@gl_border`, 10 px radius, 14 px padding |
| Flush card | `.gl-card.flush` | 0 padding, `set_overflow(HIDDEN)` |
| Card head | `.card-head` | 12/14/8/14 px |
| Section label | `.section-label` | mono, 0.78em, 600, 0.12em spacing, `@gl_muted`, caps |
| Muted | `.muted`/`.subtle`/`.small` | `@gl_muted`/`@gl_subtle`/0.85em |
| Mono | `.mono` | mono stack, `tnum` |
| Primary | `button.gl-primary` + `suggested-action` | accent background, accent fg, 600 |
| Small | `button.gl-small` | 0 height, 2/10 px, 0.85em |
| Chip | `.gl-chip.positive/.neutral/.negative` | mono, 0.78em, 600, 1/8 px, 6 px radius |
| Notice | `.gl-notice` | see §7 |
| Hover | | `@gl_hover` |
| Selected row | | 22% accent over transparent |
| Focus | | accent outline |
| Thumbnail placeholder | | `@gl_sunken` |
| Scroller viewport | `.gl-scroller > viewport` | `background: none` |
| Split | `paned > separator` | 12 px, no background/border/shadow |
| Empty state | | centred wrapped `.muted` |

Derived colours at the top of `style.css`, exactly per the standard:

```css
@define-color gl_surface color-mix(in srgb, @window_fg_color 4%, @window_bg_color);
@define-color gl_sunken  color-mix(in srgb, @window_fg_color 2%, @window_bg_color);
@define-color gl_border  color-mix(in srgb, @window_fg_color 16%, @window_bg_color);
@define-color gl_hover   color-mix(in srgb, @window_fg_color 8%, @window_bg_color);
@define-color gl_muted   color-mix(in srgb, @window_fg_color 68%, @window_bg_color);
@define-color gl_subtle  color-mix(in srgb, @window_fg_color 42%, @window_bg_color);
:root { --window-radius: 0px; }
```

No hardcoded colours except the fixed status colours and the image surfaces allowed by the
standard. Status colours are chosen by the `.dark` class that ThemeWatcher sets from the
effective background luminance.

### Spacing

| Between | Value |
| --- | --- |
| Cards/sections | 12 |
| Rows in a card | 10 |
| Controls in a row | 8 |
| Heading and content | 6 |
| Field label and field | 4 |

Radii: window 0; cards 10; rows/notices 8; chips/tiles 6; thumbnails 4; stock controls use
the toolkit's own radius.

---

## 6. Dialogs

- **File/folder:** `Gtk::FileDialog`, modal, title names the task, filters for supported
  types, last folder as initial folder. Cancel is not an error.
- **Messages/confirmations:** `Gtk::AlertDialog`; heading states the question/outcome;
  detail names the specifics; buttons are verbs; the safe choice is cancel; a destructive
  choice is never the default.
- **Custom dialogs/editors** (metadata, artwork, bulk organisation, saved filters, backup):
  `Gtk::Window` subclass, transient for the main window, modal where an answer is required,
  own header bar with a wordmark title (`EDIT METADATA`, `RESTORE BACKUP`), `.dialog-body`
  18 px padding, buttons bottom right (cancel first, primary last, default, Escape cancels).
- **Progress:** a cancellable operation shows a `.gl-card` progress row inside the page, not
  a modal dialog, unless the user must not interact with the page.
- **Non-blocking results:** notices, not dialogs.

---

## 7. Notices

- A `Gtk::Revealer` (`SLIDE_UP`) centred at the bottom of a `Gtk::Overlay`, with
  `set_can_target(false)`.
- One wrapped label, at most 80 characters wide.
- `margin: 0 0 16px 0; padding: 9px 16px;`, 8 px radius, `@gl_border`, `@gl_surface`,
  `@window_fg_color`, shadow `0 2px 10px alpha(black, 0.18)`.
- Status class tints text, border (45% mix) and background (8% mix).
- Timeout: 5 s routine, 10 s if the user may want to read a path, none for progress.
- A new notice replaces the previous one. No queue that hides failures.

---

## 8. Keyboard shortcuts

| Shortcut | Action | Scope |
| --- | --- | --- |
| Ctrl+Q | Quit | all |
| Ctrl+Shift+M | Toggle system title bar | all |
| Alt (tap) | Open/close main menu | all |
| F10 | Open main menu | all |
| F1 | Help | all |
| Ctrl+R | Refresh all sources | library |
| Ctrl+F | Focus search | library |
| Ctrl+D | Settings | all |
| Ctrl+M | Toggle reduced motion | all |
| Ctrl+O | Add ROM folder… | library |
| Ctrl+Shift+S | Save backup… | library/details |
| Ctrl+A | Select all (where a selection exists) | lists |
| Delete | Remove selected (with confirmation) | lists |
| Menu key | Context menu | lists |
| Escape | Close menu/dialog; back one page; not quit | all |
| F11 | Toggle Game Mode / leave panel | all |
| Ctrl+Print | Save a clip (P3, if enabled) | all |

Rules:

- Window-wide shortcuts use `application->set_accel_for_action()` against named actions.
- Focus-only shortcuts use a `Gtk::ShortcutController` with `Gtk::NamedAction` on the widget.
- Navigation handlers return `false` for Ctrl/Alt combinations so accelerators keep working.
- Do not reuse a standard shortcut for a different action.
- Controller buttons map to the same named actions, never to duplicated key events.

---

## 9. About dialog

A single `Gtk::AboutDialog`, created on first use and kept (`set_hide_on_close(true)`),
transient for and modal to the main window.

| Field | Value |
| --- | --- |
| Program name | LGL Games Library |
| Version | `LGL_GAMES_LIBRARY_VERSION` |
| Comments | “One local library for the games you already own.” then a blank line and “Built with GTK 4 and libadwaita.” |
| Website | project page, label `Project page and issue tracker` |
| Authors | LinuxGamerLife and the Omakade contributors; original Omakade by Tyler South |
| Credits | “Inspired by Omakade” with the upstream URL; “Support” with the LGL Ko-fi link |
| Licence | `Gtk::License::GPL_3_0` (the app is GPL-3.0-or-later) |
| Logo | bundled app icon texture |

The About dialog must not claim endorsement by the Omakade maintainer.

---

## 10. ThemeWatcher and Noctalia

`src/ui/themewatcher/` with pure, tested helpers in `src/core/theme/`:

- Watch `~/.config/gtk-4.0/` and every local `@import` folder; reload user `gtk.css` into
  the `USER` provider on change, debounced 200 ms.
- Re-apply on `AdwStyleManager` `notify::dark`/`notify::accent-color` and
  `org.gnome.desktop.interface` changes.
- No-portal fallback: derive dark mode from `color-scheme`,
  `gtk-application-prefer-dark-theme` or a `-dark` theme name.
- Apply the desktop monospace font to every class whose CSS uses the mono stack.
- Set `.dark` on the main window from the effective background luminance.
- Ignore parse errors in the user's `gtk.css`; report app `style.css` errors with line
  numbers.
- Emit a change signal so secondary windows update copied theme values.

On KineticWE, Noctalia owns the palette and its template system writes the GTK 3/4 theme and
color scheme. The application does **not** read Noctalia's private config or duplicate its
palette. It follows the resulting GTK theme, exactly like any other GTK app, and ThemeWatcher
handles the live reload that GTK does not. If Noctalia changes the palette, the compositor
and the app update from the same generated theme.

Priorities: app `style.css` at `APPLICATION`, mono rules at `APPLICATION + 1`, user
`gtk.css` at `USER`.

### Noctalia integration boundaries

- Notifications are `GNotification`; Noctalia is the session notification daemon.
- Game Mode DND uses `noctalia-kwe msg notification-dnd-status` and
  `notification-dnd-set`; the previous value is restored on exit.
- The app never loads Noctalia plugins, never writes `~/.config/noctalia-kwe/`, and never
  executes a Noctalia command built from a game title, path or other untrusted value.
- If Noctalia is absent, notifications still use the freedesktop daemon if one exists; DND
  controls are disabled with a reason.

---

## 11. Responsive behaviour

| Width | Behaviour |
| --- | --- |
| ≥ 1200 px | Sidebar visible; 5–7 columns; details as a split pane |
| 800–1199 px | Sidebar collapsible; 3–5 columns; details full page |
| 480–799 px | Sidebar hidden behind a menu; 2–3 columns; header actions collapse into menu |
| < 480 px | 1–2 columns; list view preferred; settings pages stack |

The window minimum size is 480×360. The layout must survive 200% scaling and a 1366×768
laptop.

---

## 12. Accessibility

- Every interactive control has an accessible label and, where needed, a description.
- Keyboard focus is always visible and never trapped.
- The focus order follows visual order; the controller focus engine follows rendered
  positions.
- No information is conveyed by colour alone; status chips also use text.
- Reduced motion disables reveals, animated focus and non-essential transitions.
- Text scales with the system font; no fixed pixel font sizes except relative `em`.
- High-contrast mode and the dark/light theme are supported through named colours.
- Screen reader checks use `accerciser` and `Gtk::Accessible` properties during Phase 8.

---

## 13. Snapshot smoke test

Support `LGL_GAMES_LIBRARY_GTK_SNAPSHOT=<folder>`:

- run `NON_UNIQUE` so the app does not hand off to a running instance;
- render every visible window to `<folder>/<title>.png` after 3 seconds, replacing
  non-alphanumeric title characters with `-`;
- quit.

Use a temporary `XDG_CONFIG_HOME`/`XDG_DATA_HOME` to exercise states such as system title
bar, empty library, populated fixture, details and settings. The full test plan is in
[12-Testing](12-TESTING.md).

---

## 14. Out of scope for the UI

- Omarchy shell surfaces, Quickshell components and Hyprland-specific animation.
- Embedding or imitating Noctalia’s shell surfaces; the app is a normal GTK client.
- A QML-like declarative style. GTK layout is explicit C++.
- Adwaita widgets. libadwaita is used only for initialisation and style management.
- A web view or embedded browser.
- Custom window decorations beyond the standard header bar and KineticWE's system title bar.
