# Security policy

**Do not open a public issue for a vulnerability.**

Report privately to the maintainer through the project’s security advisory channel or the
contact listed on the project page. Include:

- affected version/commit;
- the shortest reproduction;
- the trust boundary involved (launcher file, archive, network, IPC, compositor, controller);
- whether a secret, save file or desktop state is exposed;
- any suggested fix.

You will get an acknowledgement, a severity assessment and a coordinated disclosure plan.

## Scope

In scope:

- command execution, path traversal, symlink, archive and parser vulnerabilities;
- secret exposure in settings, database, logs, argv or diagnostics;
- save/restore data loss or overwrite;
- IPC, KWin script, Noctalia IPC and controller-helper issues;
- privilege escalation (there should be none);
- dependency/supply-chain issues in this project.

Out of scope:

- a game, launcher, emulator or compositor vulnerability unrelated to this app;
- a compromised root/kernel/session;
- physical access to an unlocked session;
- social engineering;
- Noctalia plugin code (plugins are trusted shell code and not loaded by this app).

## Design

The full threat model, trust boundaries, rules (`SEC-*`) and secure design review are in
[`docs/09-SECURITY.md`](docs/09-SECURITY.md). Security requirements override every other
project document.

## Supported versions

Only the latest release receives security fixes during the pre-1.0 phase. After 1.0, the
current stable minor receives fixes; older minors are supported at the maintainer’s discretion
and are listed in the release notes.

## No secrets in reports

Do not paste real API keys, passwords, ROM keys, save contents or personal paths. Redact them;
the maintainer can request a secure transfer if needed.
