# 21 — Guide overlay blocker

**Status: confirmed limitation.** KineticWE does not provide `zwlr_layer_shell_v1` to clients
in the target build. Therefore a GTK 4 layer-shell overlay cannot be placed above a fullscreen
game, and the in-game guide is **not available** on the target.

This replaces the earlier “spike” plan. Do not run a GTK-layer-shell experiment expecting it
to work, and do not add `gtk4-layer-shell` as a dependency.

---

## 1. What was wrong in the earlier draft

The earlier draft said that because the KineticWE source tree contains
`src/layershellv1integration.cpp` and a `zwlr_layer_shell_v1` implementation, `gtk4-layer-shell`
could place an overlay above a fullscreen game. That conflated two different things:

- a protocol implementation existing somewhere in the compositor source;
- the protocol being exposed and supported for client applications in the target build.

The target does not provide the protocol to clients. The claim was therefore wrong.

---

## 2. Consequence

| Item | Result |
| --- | --- |
| `gtk4-layer-shell` dependency | Not used, not packaged |
| Guide overlay process | Not built |
| Home/Capture guide action | No overlay; only actions that do not need one |
| Normal fullscreen guide window | Not a substitute; can steal input from the game |
| UI | Reports the guide as unavailable |
| Future | Requires KineticWE to expose a supported overlay surface |

---

## 3. What would make the guide possible

A future guide needs all of the following:

1. KineticWE exposes a documented, supported client overlay protocol or API.
2. The overlay can stack above a fullscreen game reliably.
3. Keyboard/pointer focus can be acquired and released without leaving the game stuck.
4. The behaviour is stable across native, Proton and gamescope sessions.
5. A new design and security review covers input capture, payload validation, crash recovery
   and the actions the overlay may invoke.
6. The packaging adds the dependency only when the feature is built and tested.

None of these is assumed today.

---

## 4. What not to do

- Do not add `gtk4-layer-shell` “just in case.”
- Do not ship an overlay that cannot map; a silent no-op is worse than an honest unavailable
  state.
- Do not use a normal fullscreen window and call it an overlay.
- Do not load a Noctalia plugin to fake an overlay; plugins are trusted shell code and outside
  this app’s trust boundary.
- Do not re-use the Omarchy Quickshell plugin; it cannot run on Noctalia.

---

## 5. User-facing message

When the user asks for the guide (Home button, menu item, shortcut):

> The in-game guide is not available on KineticWE because the compositor does not provide an
> overlay surface. Game Mode and the library still work.

Diagnostics records the missing capability. No error dialog is required unless the user
explicitly invoked the guide.

---

## 6. Verification

- [ ] No `gtk4-layer-shell` in CMake, spec, docs, or runtime.
- [ ] No overlay process or guide UI in the build.
- [ ] The UI/help says the guide is unavailable.
- [ ] Home/Capture actions that remain work and do not claim to open a guide.
- [ ] A future change to this document includes the compositor version and a new security
      review.
