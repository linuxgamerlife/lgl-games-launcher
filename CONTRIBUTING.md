# Contributing

This repository is in the design phase. Contributions to the plan are welcome; code starts
only after the P0 scope and security review are approved.

## Before you start

1. Read `README.md`, `docs/README.md` and `docs/09-SECURITY.md`.
2. Check `docs/17-RISKS-DECISIONS.md` for decisions and open questions.
3. For a non-trivial change, open an issue or draft proposal describing:
   - the user problem;
   - the proposed behaviour;
   - assets and trust boundaries;
   - limits and failure modes;
   - tests;
   - which document(s) change.

## Design contributions

- Keep the numbered document structure.
- Use the terminology in `docs/20-GLOSSARY.md`.
- Do not weaken a security requirement. If a design needs a deviation, write it in
  `docs/17-RISKS-DECISIONS.md` with an owner, reason and removal plan.
- Do not present a planned feature as implemented.
- Cite the upstream file/behaviour when describing Omakade parity.

## Code contributions (after Phase 1 approval)

- C++20, gtkmm 4, GLib/GIO, no Qt/KF6/QML.
- Follow `docs/16-CODING-STANDARDS.md`.
- Core logic in `src/core` with tests; no widgets in core.
- Every external input validated; no shell; no secret outside Secret Service.
- Add tests for normal, boundary and malicious input.
- Run the build and tests in the Fedora 44 container; report exact commands.
- Update docs, feature matrix and changelog in the same pull request.

## Commit and pull request

- One logical change per commit.
- Explain the behaviour, the security impact and the tests run.
- Include a screen recording or snapshot for UI changes where possible.
- Do not commit build directories, generated resources, credentials, ROMs, keys, saves or
  personal data.

## Licence

By contributing, you agree your contribution is licensed under GPL-3.0-or-later, consistent
with the upstream Omakade licence and the project. Console artwork remains under its own
CC BY-SA 3.0 notice.

## Reporting security issues

Do not open a public issue for a vulnerability. See `SECURITY.md`.
