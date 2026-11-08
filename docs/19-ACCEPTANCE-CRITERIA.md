# 19 — Acceptance criteria and definition of done

A feature is “done” only when every applicable item below is true. A release ships only when
its milestone gate in [08-Roadmap](08-ROADMAP.md) and the release gate here are met.

---

## 1. Feature definition of done

### Design

- [ ] The feature is in [02-Scope](02-SCOPE.md) and [06-Feature parity](06-FEATURE-PARITY.md).
- [ ] A short design note states the user outcome, assets, trust boundaries and failure modes.
- [ ] Security review in [09-Security](09-SECURITY.md) covers it.
- [ ] Settings keys, paths, limits and errors are documented.

### Implementation

- [ ] Core logic is in `src/core` and has no widget headers.
- [ ] No Qt/KF6/QML dependency.
- [ ] All external input is validated and bounded.
- [ ] No shell, no secret in logs/database/settings, no symlink following where unsafe.
- [ ] Errors are handled; no ignored result.
- [ ] Ownership, lifetime and thread affinity are documented.
- [ ] UI follows the GTK standard components, colours and spacing.
- [ ] Keyboard, mouse and controller paths are reachable.
- [ ] Accessibility names/descriptions exist.

### Tests

- [ ] Unit tests cover normal, boundary and malicious input.
- [ ] A regression test exists for the bug/feature.
- [ ] Integration test uses a temp XDG root and a synthetic fixture.
- [ ] UI snapshot exists for a new page/dialog.
- [ ] Controller navigation is covered where relevant.
- [ ] Fuzz target exists for a new parser/archive.
- [ ] Tests pass on GCC and Clang, Debug and Release.

### Quality

- [ ] `-Werror` clean.
- [ ] clang-tidy/cppcheck clean or documented false positives.
- [ ] ASan/UBSan clean for the touched code.
- [ ] No High/Critical security finding.
- [ ] Performance budget met or a documented exception.

### Documentation

- [ ] README/HELP updated if user-visible.
- [ ] `CHANGELOG.md` entry.
- [ ] Feature matrix row updated.
- [ ] Data/privacy/security docs updated if data or endpoints changed.
- [ ] Packaging/docs paths updated.

### Release

- [ ] Version updated in spec, CMake, metainfo, changelog.
- [ ] RPM/COPR build passes.
- [ ] Desktop/metainfo validate.
- [ ] SBOM/checksums/signature.
- [ ] Known limitations stated.

---

## 2. P0 acceptance (0.1)

### Functional

- [ ] Discovers Steam, Heroic, Lutris, GOG, RetroArch, ROM folders and manual games.
- [ ] Unified grid/list with search, source filter, installed filter and sort.
- [ ] Details page with local artwork and source/path.
- [ ] Settings persist and import Omakade `config.toml`.
- [ ] Existing Omakade library/artwork import with unchanged source data.
- [ ] Single instance; second launch raises the window.
- [ ] Graceful empty state and source warnings.

### Quality

- [ ] 200-game fixture renders at 480/800/1280/2560.
- [ ] Search ≤ 200 ms at 1,000 games.
- [ ] No main-loop block > 50 ms.
- [ ] Malformed files never crash.
- [ ] Snapshot tests for empty/populated/list/details/settings.

### Security

- [ ] No shell execution.
- [ ] Paths validated; source reads read-only.
- [ ] Secret Service detection; no plaintext secret.
- [ ] No High/Critical finding.

---

## 3. P1 beta acceptance (0.5)

- [ ] Organisation (favourites, hidden, tags, collections, saved filters, links) survives
  restart and backup.
- [ ] Artwork editor and reset.
- [ ] Metadata/achievement connections work, cache offline, and never leak secrets.
- [ ] Backup export/import and preview; Omakade v1/v2 compatible.
- [ ] Save protection with snapshot/restore/recovery and coverage UI.
- [ ] Remaining local sources have fixtures.
- [ ] Full statistics and Year in Review PNG.
- [ ] All settings pages complete and keyboard reachable.
- [ ] Controller navigation and fullscreen Game Mode on the current display.
- [ ] Performance budgets met.

---

## 4. P2 / 1.0 acceptance

- [ ] Game Mode dedicated display, audio and DND on KineticWE, with journal/recovery.
- [ ] Hold launcher windows and desktop restoration, or a documented capability gap.
- [ ] Controller Home/Capture where enabled.
- [ ] Guide reported unavailable on KineticWE; no overlay dependency or code ships.
- [ ] Optional recorder only with a separate security review and consent.
- [ ] Accessibility audit completed.
- [ ] 10,000-game benchmark met.
- [ ] Fallback session (stock KDE or GNOME) runs the library and explains missing features.
- [ ] No High/Critical security finding.
- [ ] Signed release, checksums, SBOM.

---

## 5. Security release gate

A release is blocked if any of these is true:

- [ ] Any High or Critical finding from CodeQL, clang-tidy, cppcheck, sanitizers, fuzzing or
      dependency scanning is unresolved.
- [ ] A secret can be written to a non-secret store or logged.
- [ ] A code path can execute a shell or an arbitrary program selected by untrusted data.
- [ ] A path traversal, zip-slip, symlink or archive bomb is reproducible.
- [ ] A secret/service/API credential appears in an artifact or log.
- [ ] A restore can delete or overwrite a live save outside the recorded set.
- [ ] A KWin script or Noctalia command can be injected from data.
- [ ] A future overlay (if KineticWE adds support) can leave input grabbed after exit/crash.
- [ ] The release artifact cannot be reproduced/verified.
- [ ] A known vulnerability is unpatched without a documented, approved risk acceptance.

---

## 6. Privacy release gate

- [ ] No telemetry; no hidden request at startup.
- [ ] Every network endpoint is documented in [10-Privacy](10-PRIVACY.md).
- [ ] Credentials are only in Secret Service.
- [ ] Removing a connection removes its secret.
- [ ] Diagnostics are opt-in and redacted.
- [ ] No ROM/save/library upload.
- [ ] Noctalia/KineticWE integration is documented and read-only.
- [ ] Recording (if any) is opt-in with an indicator and limits.

---

## 7. Packaging release gate

- [ ] Spec builds in a clean Fedora 44 chroot/COPR.
- [ ] No Qt/KF6 dependency.
- [ ] `desktop-file-validate` and `appstreamcli validate --no-net` pass.
- [ ] Icons 48/64/128/256 present.
- [ ] systemd units verify and start.
- [ ] Source archive excludes `legacy/qt/`.
- [ ] Binary RPM installs, launches, uninstalls.
- [ ] SBOM, `SHA256SUMS`, signature.
- [ ] Release notes list tested KineticWE/Noctalia/Fedora versions and limitations.

---

## 8. Documentation release gate

- [ ] README status matches reality.
- [ ] HELP covers every shortcut and source.
- [ ] CHANGELOG dated and complete.
- [ ] Feature matrix updated.
- [ ] Security/privacy docs updated.
- [ ] No active doc refers to `gtk/src/`, Qt, QML, Omarchy or Hyprland as current.
- [ ] No document claims an unavailable overlay or an unverified recorder works.

---

## 9. Sign-off

| Role | Name | Date | Scope |
| --- | --- | --- | --- |
| Maintainer | | | scope, release |
| Security reviewer | | | threat model, findings |
| Packager | | | RPM/COPR, metadata |
| QA | | | manual acceptance |

A sign-off is not a substitute for the automated gates. Any open exception is listed with an
owner, expiry and removal plan in [17-Risks and decisions](17-RISKS-DECISIONS.md).
