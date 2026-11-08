# 09 — Security and secure design review

**Status: mandatory.** The user has stated that security is the one standard that is not
flexible. Where this document conflicts with convenience, performance, schedule or any other
project document, this document wins. A deviation requires a written risk acceptance approved
by the maintainer and recorded in [17-Risks and decisions](17-RISKS-DECISIONS.md).

This document satisfies the LGL [Secure Design Review Standard](../../docs/secure-design-review-standard.md)
and the [Privileged Operations Standard](../../docs/privileged-operations-standard.md).
The application performs **no privileged operations** and ships **no root helper**. If that
ever changes, the privileged standard applies in full and this review must be repeated.

---

## 1. Security objectives

1. A game, launcher file, ROM, archive, image, network response or IPC message must never lead
   to code execution beyond the user's own explicit launch action.
2. Personal data (library, history, saves, credentials) stays local and is never exposed to
   another user or to a network service the user did not enable.
3. Secrets are stored only in the Secret Service and never appear in logs, argv, settings,
   the database or crash output.
4. A failed operation fails closed and leaves the user's data and desktop state recoverable.
5. The application cannot be used to escalate privilege, escape a sandbox, or damage files
   outside the paths the user selected.
6. Updates and releases are verifiable.

---

## 2. Assets

| Asset | Why it matters | Where it lives |
| --- | --- | --- |
| Library database | Personal game list, organisation, history | `$XDG_DATA_HOME/lgl-games-library/library.sqlite3` |
| Play history | Personal behaviour over time | same database |
| Save sets | Irreplaceable game progress | `$XDG_DATA_HOME/lgl-games-library/save-sets/` |
| Backup archives | Portable personal data | user-chosen path |
| Credentials | Account/API access | Secret Service only |
| Secret Service session | Other apps' secrets | D-Bus session |
| User files | ROMs, game installs, saves | arbitrary user paths |
| Desktop session | Windows, outputs, audio, notifications | KineticWE/Noctalia |
| Running processes | Games and emulators | `/proc`, compositor |
| Application binary and data | Integrity of what runs | install prefix |
| Logs | Diagnosis | stderr/journal |

---

## 3. Trust boundaries

```text
[ untrusted files ] ──► source scanners ──► validated records ──► SQLite
[ untrusted network ] ──► HTTP client ──► validated models ──► SQLite/cache
[ untrusted archives ] ──► backup/save readers ──► validated payloads ──► filesystem
[ untrusted ROM/headers ] ──► identity parsers ──► validated metadata ──► UI
[ user UI ] ──► launch planner ──► argv ──► OS process
[ GUI ] ──► private socket ──► sessiond / guide / guide-button
[ app ] ──► KWin D-Bus/script / Noctalia IPC ──► desktop
[ app ] ──► Secret Service
```

Every arrow is a validation point. “Validated” means a named function with tests, not an
inline `if`.

| ID | Boundary | Input | Validator | Failure behaviour |
| --- | --- | --- | --- | --- |
| TB-1 | Launcher file → scanner | VDF, ACF, YAML, JSON, TOML, desktop entry, SQLite | per-source parser with size/depth caps | skip record, warn, keep previous cache |
| TB-2 | Network → model | JSON/image/HTTP headers | schema, type, length, host, TLS, size | reject, keep cache, notice |
| TB-3 | Archive → filesystem | ZIP entries, manifests | allowlist, hash, size, path, type | reject whole archive, no extraction |
| TB-4 | ROM/header → metadata | bytes | magic, offsets, length, bounds | unknown, no crash |
| TB-5 | UI → process | argv/program/env | allowlist, path, type, no shell | refuse with message |
| TB-6 | GUI ↔ daemons | JSON lines over Unix socket | schema, length, peer uid, allowlist | drop connection, log |
| TB-7 | App → KWin | D-Bus calls / script | capability probe, allowlist, UUID/path validation | no-op with reason |
| TB-8 | App → Noctalia | `noctalia-kwe msg` | fixed command, timeout, output cap | leave state unchanged |
| TB-9 | App → Secret Service | secret read/write | libsecret, no logging | memory-only, warn |
| TB-10 | Controller → app | evdev events | read-only, device allowlist, bounded | no-op |
| TB-11 | Recorder → filesystem | captured output | path under chosen root, size cap, no symlink | stop, report |
| TB-12 | Settings → app | INI values | type, bounds, enum | default |

---

## 4. Threat actors and assumptions

| Actor | Capability | In scope? |
| --- | --- | --- |
| Malicious launcher/ROM/archive file | Crafted local file opened by the user | **Yes** |
| Malicious network service | Controls responses from a provider | **Yes** |
| Another local user | Cannot read the user's files; may send IPC if permissions are wrong | **Yes** |
| Malicious game process | Same user, can read the user's files | **No** — the game runs as the user and is trusted to that extent |
| Root/kernel attacker | Full control | **No** |
| Compromised dependency | Supply-chain attack | **Yes**, via pinning, review and scanning |
| Physical attacker | Access to an unlocked session | **No** |
| User misconfiguration | Chooses a bad path/command | **Yes**, with clear warnings and safe defaults |

The app does not try to sandbox games. It must never run a game or external tool with more
privilege than the user already has, and never with a shell.

---

## 5. Security requirements (normative)

The identifiers are used in tests, code comments and review checklists.

### 5.1 Privilege

- `SEC-PRIV-01` The GUI, daemons and helpers run as the invoking user. No `setuid`, `setgid`,
  capabilities, polkit action or root helper.
- `SEC-PRIV-02` No code path calls `sudo`, `pkexec`, `systemctl` as root, or writes outside the
  user's own XDG/home paths except a path the user explicitly selects.
- `SEC-PRIV-03` If a future feature needs privilege, it stops here and requires a new design
  review under the Privileged Operations Standard.

### 5.2 Process execution

- `SEC-EXEC-01` Never use `system()`, `popen()`, `execl*` with a shell, `bash -c`, `sh -c`, or
  string concatenation to build a command.
- `SEC-EXEC-02` Every launch is a `LaunchPlan` with program, argv vector, environment and
  working directory; the executor is the only code that starts a process.
- `SEC-EXEC-03` Programs are resolved to an allowed executable for the source; Flatpak app IDs
  match `^[A-Za-z0-9._-]{1,255}$` and are passed as one argument.
- `SEC-EXEC-04` User manual commands are parsed into argv without a shell, shown before first
  use, and labelled as user-supplied. They may not contain NUL, newline or control characters.
- `SEC-EXEC-05` Environment is an allowlist plus documented variables. API keys, passwords and
  tokens are never placed in argv or environment of a game.
- `SEC-EXEC-06` External tools have a timeout, bounded stdout/stderr capture (for example
  64 KiB), and an explicit exit-status result.
- `SEC-EXEC-07` Args derived from a game title, path or network value are passed as single
  argv entries and validated; they are never split or interpolated.
- `SEC-EXEC-08` No downloaded content is executed. Native-port catalogue links are opened in a
  browser, not run.

### 5.3 File and path handling

- `SEC-PATH-01` Treat every path from a launcher, archive, database or network as untrusted.
- `SEC-PATH-02` Require absolute paths after normalisation; reject `..`, empty segments, NUL,
  control characters, overlong paths (> 4096) and paths outside the expected root where a root
  is defined.
- `SEC-PATH-03` Canonicalise with `realpath` only after the path is known to exist; do not
  follow symlinks when a save/restore target is expected to be a regular file.
- `SEC-PATH-04` Refuse to read or write through a symlink for restore, save-set and backup
  operations (`lstat`/`openat`/`O_NOFOLLOW` where available).
- `SEC-PATH-05` Create directories 0700 and private files 0600. Exported backups/clips are 0600
  unless the user chooses otherwise.
- `SEC-PATH-06` Write atomically: temp file in the same directory, write, `fsync`, `rename`.
  Never truncate a good file before the replacement is ready.
- `SEC-PATH-07` Bound directory traversal by depth, entry count and total bytes; never follow
  directory symlink loops.
- `SEC-PATH-08` Never delete a path the user did not select or that is not inside the app's own
  data root; save restore never deletes live files outside the recorded set.

### 5.4 Archives and backups

- `SEC-ARCH-01` Read archives into memory or a bounded temp file; never extract by entry path.
- `SEC-ARCH-02` Reject absolute paths, `..`, drive letters, backslashes used as separators,
  empty names, duplicate names, hard links, symlinks, devices, FIFOs and encrypted entries.
- `SEC-ARCH-03` Accept only Store or Deflate; enforce per-entry and total uncompressed limits
  and a compression-ratio cap to stop zip bombs.
- `SEC-ARCH-04` Validate the manifest schema, version, table/field/setting allowlist and
  record counts before touching the database.
- `SEC-ARCH-05` Validate every artwork/content hash and image type; reject a hash mismatch.
- `SEC-ARCH-06` Restore in a transaction after a validated in-memory payload; write a recovery
  copy first.
- `SEC-ARCH-07` Fuzz every archive/manifest parser; a crash or hang blocks release.

### 5.5 Network

- `SEC-NET-01` HTTPS only for remote services; validate the system trust store and hostname.
  Never disable certificate verification.
- `SEC-NET-02` Allowlist hosts per provider. Redirects are limited to the same host or a
  documented image host and are re-validated.
- `SEC-NET-03` Set connect and read timeouts; cap response bytes (for example 8 MiB for JSON,
  32 MiB for an image) and cap decompressed size.
- `SEC-NET-04` Validate `Content-Type` and parse with size/type/depth limits. Reject unknown
  fields only where the schema says so; ignore unknown fields otherwise.
- `SEC-NET-05` Never send credentials in a URL or query string. Use headers or the provider’s
  OAuth flow.
- `SEC-NET-06` Never log request headers, query strings containing a key, tokens, cookies or
  full response bodies.
- `SEC-NET-07` Network features are opt-in where they identify the user; public artwork can be
  on by default with a documented host list and an off switch.
- `SEC-NET-08` A user-configured URL (for example RomM) must be `http`/`https`, have a host,
  and no embedded username/password. Private/LAN hosts are allowed because that is the use
  case; the UI states what is sent.
- `SEC-NET-09` Do not fetch or execute remote code, plugins, themes or scripts. Templates are
  data; the app does not apply Noctalia or Omarchy templates.
- `SEC-NET-10` Offline mode disables all HTTP, including update checks (there is no updater).

### 5.6 Secrets

- `SEC-SEC-01` Store credentials only with libsecret in the Secret Service.
- `SEC-SEC-02` Never write a secret to `settings.ini`, the database, a cache, a log, a crash
  report, argv, an environment variable of a child process, or a UI label.
- `SEC-SEC-03` Read a secret only when needed, keep it in memory for the shortest time, and
  clear the buffer where practical.
- `SEC-SEC-04` If the Secret Service is unavailable, keep the secret in memory for the session
  only and tell the user it will not persist. Do not fall back to plaintext.
- `SEC-SEC-05` Removing a connection deletes its Secret Service entries. The UI says what was
  removed.
- `SEC-SEC-06` Logs and diagnostics redact secret-looking values and never include request
  headers.

### 5.7 IPC and local services

- `SEC-IPC-01` The control socket lives in a 0700 `$XDG_RUNTIME_DIR` directory with mode 0600.
- `SEC-IPC-02` Verify the peer UID equals the current UID where the platform exposes it.
- `SEC-IPC-03` Cap a line at 4 KiB and a message at 8 KiB; reject malformed JSON and unknown
  commands.
- `SEC-IPC-04` Commands are a fixed allowlist. No command executes a player-supplied program,
  path or shell string.
- `SEC-IPC-05` No guide payload exists on KineticWE. If a future compositor supports an
  overlay, its payload must be read-only data with a schema and fixed actions; the overlay must
  not invoke arbitrary commands.
- `SEC-IPC-06` The guide-button helper reads evdev only. It never writes, injects input, or
  opens a device it did not identify as a gamepad.
- `SEC-IPC-07` The recorder writes only to the chosen video root and its own temp directory;
  filenames are generated, not taken from a title without sanitisation.

### 5.8 Desktop integration

- `SEC-DESK-01` Probe KineticWE capabilities before use; never assume a D-Bus method exists.
- `SEC-DESK-02` The bundled KWin script is a constant file in the GResource. It is loaded by a
  path the app controls, supports only named operations, and never accepts script source from
  the database, network, IPC or user input.
- `SEC-DESK-03` Validate window UUIDs/addresses and output names before passing them to KWin.
  Reject strings outside the documented shape.
- `SEC-DESK-04` Enabling/disabling an output or changing audio affects the user's session:
  require confirmation when it could leave the user with no display or no sound, and journal
  the previous state first.
- `SEC-DESK-05` Noctalia commands are fixed strings, run with argv, a timeout and an output
  cap. DND state is saved and restored; failure leaves it unchanged.
- `SEC-DESK-06` Never load Noctalia plugins, execute Luau, or write into Noctalia's config.
- `SEC-DESK-07` No overlay is used on KineticWE because layer-shell is unavailable. If a future
  compositor adds a supported overlay, it must not grab input permanently; release keyboard and
  pointer on hide, game exit, crash and session end, and require a new security review.
- `SEC-DESK-08` A recorder must show an indicator, require explicit enablement, and never
  capture audio or the screen without the user's configured consent.

### 5.9 Database and settings

- `SEC-DB-01` Prepared statements only. No user value is concatenated into SQL.
- `SEC-DB-02` Table/column names are compile-time constants.
- `SEC-DB-03` Treat a database as untrusted: validate row types, lengths and counts; never
  deserialize a blob into code.
- `SEC-DB-04` Migrations are transactional and bounded; keep a pre-migration copy.
- `SEC-DB-05` Settings validate type, range and enum; malformed values fall back to defaults.
- `SEC-DB-06` Cap list entries (ROM folders 64, saved filters 500, artwork files 4,096, etc.)
  and file sizes.

### 5.10 Logging and diagnostics

- `SEC-LOG-01` Never log secrets, tokens, cookies, passwords, private keys, ROM keys, save
  contents, full personal paths or network response bodies.
- `SEC-LOG-02` Log the operation, the target (redacted where needed), the result and a bounded
  error. Do not log argv containing a secret.
- `SEC-LOG-03` A diagnostics export is user-initiated, shows what it contains, and excludes
  secrets and personal data by default.
- `SEC-LOG-04` A crash must not dump memory or environment to a world-readable file.

### 5.11 Supply chain and releases

- `SEC-SUP-01` Pin dependency versions where practical; use Fedora packages for the shipped
  build.
- `SEC-SUP-02` CI runs dependency review, CodeQL, clang-tidy, cppcheck, warnings-as-errors,
  ASan/UBSan and tests.
- `SEC-SUP-03` Releases are built in CI/COPR, signed, and published with SHA-256 checksums and
  an SBOM.
- `SEC-SUP-04` High/Critical findings block a release; Medium findings are reviewed; Low
  findings are tracked.
- `SEC-SUP-05` No build downloads and executes arbitrary code. Generated resources and
  vendored fixtures are reviewed.

---

## 6. Per-feature design review

Each feature is reviewed against the standard’s questions: malicious input, command failure,
misuse, least privilege, allowlist, input validation.

### 6.1 Source scanning

- **Assets:** launcher files, ROM folders, library DB.
- **Input:** untrusted local files, directory trees.
- **Misuse:** a crafted manifest/path tries to escape a root, exhaust memory, or point at a
  sensitive file. A symlink loop stalls a scan.
- **Controls:** `SEC-PATH-01..08`, per-source size/depth caps, read-only, transaction per
  source, no writes to launcher dirs, skip unreadable entries, bounded log.
- **Failure:** skip the record, keep previous cache, show a source warning.

### 6.2 Network metadata and artwork

- **Assets:** credentials, library, cache, user IP/privacy.
- **Input:** HTTP responses, images, JSON.
- **Misuse:** malicious server returns a huge/decompression-bomb image, a redirect to a
  different host, an HTML page, or a crafted JSON causing a parse hang.
- **Controls:** `SEC-NET-01..10`, image pixel/byte limits, JSON parser limits, host allowlist,
  no code, no remote templates, caches preserved on failure.
- **Failure:** reject, keep previous cache, notice, no retry storm (bounded backoff).

### 6.3 Launch

- **Assets:** process execution, user files, session.
- **Input:** installation records, manual command, paths, Flatpak IDs.
- **Misuse:** a malicious library row or network value becomes an argument to a shell; a path
  points to an unexpected program; a Flatpak ID is malformed.
- **Controls:** `SEC-EXEC-01..08`, allowlist, argv, no shell, validate program/path, explicit
  user action for manual commands, no secrets in argv/env.
- **Failure:** refuse with an actionable message; never fall back to a shell.

### 6.4 Session recorder

- **Assets:** play history, database integrity, process visibility.
- **Input:** `/proc`, compositor window titles, profiles, clock.
- **Misuse:** a crafted title is used as SQL/path; a process disappears mid-write; clock
  jumps; a second recorder opens the same library.
- **Controls:** prepared statements, monotonic time, ownership lock, durable journal, bounded
  title length, no title-as-path, separate connection.
- **Failure:** close at last heartbeat, replay, report storage failure, never invent time.

### 6.5 Backup and restore

- **Assets:** personal data, filesystem.
- **Input:** ZIP archive, manifest JSON, images.
- **Misuse:** zip slip, symlink entry, zip bomb, duplicate entries, hash mismatch, malicious
  image, huge manifest, unknown table that could inject SQL.
- **Controls:** `SEC-ARCH-01..07`, `SEC-DB-01..06`, in-memory validation, allowlists, limits,
  atomic restore, recovery copy, transaction.
- **Failure:** reject the archive without changing existing data; keep the old payload.

### 6.6 Save protection

- **Assets:** irreplaceable game saves, free space.
- **Input:** emulator config, save directories, manifests.
- **Misuse:** path traversal, symlink to a sensitive file, restore over a live file changed by
  the user, a huge directory, an interrupted restore, a second emulator writing mid-restore.
- **Controls:** `SEC-PATH-02..08`, allowlisted layouts, `O_NOFOLLOW`, size/file caps, journal,
  process checks, confirmation, recovery that refuses unexpected changes.
- **Failure:** abort the snapshot rather than record an incomplete set; restore rolls back or
  leaves a recovery journal; never delete live progress.

### 6.7 Achievements and provider APIs

- **Assets:** credentials, account identifiers, cache.
- **Input:** provider JSON, images.
- **Misuse:** credential leak in logs/URLs, malicious response, provider changes.
- **Controls:** `SEC-NET-*`, `SEC-SEC-*`, schema validation, cache, no credential in URL,
  bounded images.
- **Failure:** keep cache, show unavailable, never remove existing achievements.

### 6.8 Controller and guide button

- **Assets:** input devices, user session.
- **Input:** evdev events, SDL device info.
- **Misuse:** reading a non-gamepad device, stuck button, repeated commands, permission error.
- **Controls:** device class check, read-only, debounce, fixed commands, no injection.
- **Failure:** no-op, log once, tell the user which permission is missing.
- **Note:** `/dev/input/event*` access on Fedora is usually granted to the active seat; the app
  must not add a udev rule that grants world access. Document the required group/permission and
  let the user decide.

### 6.9 Game Mode and compositor control

- **Assets:** desktop session, windows, outputs, audio, notifications.
- **Input:** KWin D-Bus responses, output names, window UUIDs, user settings.
- **Misuse:** disabling the only output, moving an unrelated window, holding a window that
  never returns, a crafted output name in a script, a stuck overlay.
- **Controls:** `SEC-DESK-01..08`, capability probe, allowlist, confirmation, state journal,
  timeout, restore on crash, no untrusted script source.
- **Failure:** undo partial changes, report notes, leave desktop usable.

### 6.10 Noctalia DND and notifications

- **Assets:** user attention, notification state.
- **Input:** Noctalia IPC output.
- **Misuse:** DND left on after a crash, command injection, spoofed output.
- **Controls:** fixed argv, boolean parser, save/restore, crash recovery restores DND, bounded
  output.
- **Failure:** leave notifications unchanged; report in diagnostics.

### 6.11 Highlights / recorder (if approved)

- **Assets:** screen content, audio, disk, privacy.
- **Input:** captured frames/audio, recorder output.
- **Misuse:** recording without consent, capturing secrets/passwords, disk exhaustion, a
  malicious output path.
- **Controls:** explicit opt-in, visible indicator, chosen output, size/time caps, no audio by
  default, generated filenames, `SEC-PATH-*`.
- **Status:** deferred; requires a separate review before implementation.

### 6.12 Diagnostics export

- **Assets:** personal data, secrets.
- **Input:** logs, config, capability report.
- **Misuse:** accidental secret/path leak when shared.
- **Controls:** user preview, redaction, opt-in sections, no database dump by default.
- **Failure:** cancel leaves no file.

---

## 7. Abuse cases considered

| Abuse | Result |
| --- | --- |
| A game title contains `; rm -rf ~` | It is displayed and passed as one argv entry; no shell. |
| A crafted ACF points at `/etc/shadow` | Path is read as a launcher field, not opened as a game; launch validates the executable and content. |
| A backup has `../../.bashrc` | Rejected before extraction. |
| A backup has a 10 GiB entry | Rejected by size cap. |
| A network response redirects to `file://` | Rejected; only http(s) hosts allowed. |
| A RomM URL contains `user:pass@host` | Rejected; credentials go to Secret Service. |
| A KWin script name comes from a save file | Never; the script path is a constant GResource. |
| A Noctalia output is `on; reboot` | Parsed as a boolean, not executed. |
| A future guide payload asks to run a program | No such action; only fixed commands. |
| Two recorders start | Lock rejects the second. |
| A restore is interrupted | Journal recovers; unexpected external changes are preserved. |
| A save path is a symlink to `~/.ssh` | Rejected by `O_NOFOLLOW`/layout validation. |
| A secret is in a log | Automated secret scan fails the build. |

---

## 8. Verification

### 8.1 Required on every change

- Compile with `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang.
- `clang-tidy` (modernize, bugprone, performance, readability, cppcoreguidelines; project
  suppressions documented).
- `cppcheck` with the project suppressions file.
- Unit and integration tests.
- `git diff --check`.

### 8.2 Required in CI

- Fedora 44 container, Debug and Release.
- ASan + UBSan build and test run.
- CodeQL C++ analysis.
- Dependency review and OSV/dependency scanning.
- Secret scan over the tree and build artifacts.
- `desktop-file-validate`, `appstreamcli validate --no-net`, `rpmlint`.
- Fuzz targets with a bounded run (corpus + new inputs).
- SBOM generation.

### 8.3 Required before release

- No open High/Critical from any tool.
- Manual security review of every changed trust boundary.
- Review of new external commands, D-Bus calls, IPC commands and file paths.
- Reproduce a clean build from the source archive and verify checksums.
- Test the release candidate on KineticWE/Noctalia and a fallback session.

### 8.4 Security test cases

Maintain tests for:

- argv construction for every launch source, including titles/paths with spaces, quotes,
  newlines, `$()`, semicolons and Unicode;
- path traversal, absolute/relative, symlink and hardlink cases;
- archive limits, duplicate names, unknown tables, hash mismatch, image bombs;
- HTTP redirect/cert/type/size/timeout cases with a local test server;
- IPC malformed/large/unknown/peer cases;
- KWin/Noctalia command construction and failure;
- DND save/restore across a simulated crash;
- secret redaction in logs and diagnostics.

---

## 9. Incident response

1. **Report privately** through the project security advisory channel; do not open a public
   issue for a working exploit.
2. **Triage:** identify affected versions, feature and trust boundary. Assign severity
   (Critical/High/Medium/Low).
3. **Contain:** if a release is affected, publish an advisory and a patched release; withdraw
   or mark the affected artifact where possible.
4. **Fix:** add a regression test and a rule update if a new class of issue is found.
5. **Notify:** release notes and advisory state affected versions, impact, workaround and fixed
   version.
6. **Review:** update this document and the threat model; do not silently lower a requirement.

No secret, personal path or exploit detail goes into a public commit message before a fix is
available.

---

## 10. Compliance checklist

Before merging a feature:

- [ ] Design note written: description, assets, trust boundaries, misuse cases.
- [ ] Inputs validated at the boundary.
- [ ] Size/count/time limits chosen and tested.
- [ ] Commands use argv, no shell.
- [ ] Paths validated; symlinks handled.
- [ ] Secrets only in Secret Service.
- [ ] Logs redacted.
- [ ] Failure mode is safe and recoverable.
- [ ] Least privilege confirmed (no root).
- [ ] Tests include malicious input.
- [ ] Static analysis and sanitizers pass.
- [ ] `SEC-*` rules referenced in code/tests where relevant.
- [ ] Security review recorded in the pull request.

Before release:

- [ ] No High/Critical finding.
- [ ] Release built in CI, signed, checksummed, SBOM published.
- [ ] Privacy/security docs updated.
- [ ] Known limitations stated.
