# 16 — Coding standards

This document applies the LGL
[C++ / Qt Development Standard](../../docs/cpp-qt-development-standard.md) to a GTK/gtkmm
code base. Where the Qt standard names a Qt mechanism, the GTK equivalent below is binding.
Security rules in [09-Security](09-SECURITY.md) override convenience.

---

## 1. Language

- C++20 minimum; C++23 features only if both GCC and Clang in the Fedora 44 container support
  them and the code still builds as C++20 or has a documented fallback.
- No compiler extensions. `CMAKE_CXX_EXTENSIONS OFF`.
- `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang.
- No Qt, no KF6, no KWin headers, no QML. KineticWE/Noctalia are runtime D-Bus/IPC peers.

---

## 2. Formatting and naming

- `clang-format` with the checked-in `.clang-format`; `clang-format --dry-run -Werror` in CI.
- 2-space indent, 100-column soft limit, braces on the same line as the statement.
- Types: `PascalCase` (`GameLibrary`, `SourceScanner`).
- Functions/methods: `camelCase` (`loadSettings`, `scanSteam`).
- Members: `m_camelCase`.
- Constants: `kCamelCase` for compile-time values, `UPPER_SNAKE` only for macros.
- Namespaces: `gameslauncher`; sub-namespaces `gameslauncher::core`, `::platform`, `::ui`.
- Files: `snake_case.h/.cpp`, one primary type per file.
- GTK/GLib C types keep their C names at the boundary; wrap them in RAII early.
- Prefer descriptive names over abbreviations. No Hungarian notation.

---

## 3. Ownership and lifetime

- `std::unique_ptr` is the default owning pointer; `std::shared_ptr` only for genuine shared
  ownership, documented at the declaration.
- No owning raw pointers. No `new`/`delete` in application code.
- gtkmm widgets are owned by their parent container after `append`/`set_child`; do not delete a
  widget that has a parent.
- A widget that must survive reparenting (the header bar) is a member, not `make_managed`.
- `Glib::RefPtr` is for GObject wrappers; do not mix it with `std::shared_ptr` for the same
  object.
- Signals capture `this` only when the connection is scoped to the object's lifetime
  (`sigc::trackable`, `connect` on a member, or an explicit `sigc::connection`).
- Async callbacks capture a weak reference or a generation token; never a raw `this` that may
  die before the callback.
- A `Gio::Subprocess`/`GThreadPool`/`GTask` has an explicit owner and cancellation path.
- Document ownership in the header when it is not obvious.

### gtkmm lifetime rules

- `Gtk::Application` outlives the window; the window is a `std::unique_ptr` in `main`, reset
  after `run()`.
- A hidden window is removed from the application; `hold()`, re-`set_application()`, `present()`
  and `release()` when hiding temporarily, per the GTK standard.
- Do not keep a raw pointer to a child widget after removing it from its parent.
- `Gtk::Builder` is not used for app UI; build widgets in C++ for type safety. If used for a
  small dialog, keep the builder alive as long as the widgets.

---

## 4. Errors

- Recoverable domain errors use a result type (`std::expected<T, Error>` or a project
  `Result<T>` on C++20). Do not mix models in one subsystem.
- gtkmm/GIO throw `Glib::Error`; catch at the UI/action or service boundary and convert to a
  domain error or a user notice. Never let an exception escape a signal handler or thread.
- Exceptions are not control flow.
- Every error has a stable code, a user-facing message and a bounded diagnostic detail.
- External programs: exit status, signal, bounded stderr, timeout.
- Network: HTTP status, content type, size, timeout, bounded error.
- Never silently ignore a return value. `[[nodiscard]]` on functions whose result matters.
- A failed optional feature degrades; a failed core operation reports and stops that operation.

---

## 5. Input validation

- All external input is untrusted: files, archives, network, IPC, argv, environment, settings,
  ROM headers, launcher databases.
- Validate at the boundary; pass typed values inward.
- Bound sizes, counts, depths, lengths and time.
- Reject, do not repair silently, unless the repair is documented and safe.
- Validate enums explicitly; do not cast an integer to an enum without a range check.
- Paths: see `SEC-PATH-*`. No `..`, NUL, control chars, overlong, symlink following where not
  intended.
- JSON: JSON-GLib with depth/member limits; do not build a generic recursive walker without a
  depth cap.
- TOML/INI/VDF: bounded parser with tests and fuzz targets.
- Never use a title/path/network value as a command, SQL fragment, file name or script.

---

## 6. Threading and the main loop

- The GTK main loop owns all widgets. Worker threads never touch widgets.
- Use `std::jthread`/`GThreadPool`/`GTask` for work; deliver results with `Glib::Dispatcher` or
  `Gio::AsyncResult`.
- Every request carries a generation counter; stale results are discarded.
- Cancellation is explicit and checked at safe points.
- No blocking I/O on the main loop: files, network, archives, image decode, process waits.
- `Gio::Subprocess` waits only on a worker or with a timeout.
- Shared state is guarded or message-passed. Document the thread that owns each mutable field.
- Sanitizer runs include TSan where practical for recorder/worker code.
- Do not use `std::async` without a lifetime plan; prefer an owned thread/task.

---

## 7. GTK and gtkmm

- Use gtkmm widgets. Use libadwaita only for `adw_init()` and `AdwStyleManager`; do not use
  Adwaita widgets.
- Use named theme colours and `gl_` derived colours; no hardcoded colours outside the standard’s
  status colours.
- CSS lives in `data/style.css`, loaded from the GResource at `APPLICATION` priority; user CSS
  at `USER`.
- Widgets have accessible names/descriptions; keyboard focus is deliberate.
- Use `Gtk::ListView`/`GridView` with `Gio::ListModel`, not a `Gtk::Box` of thousands of rows.
- Use `Gtk::ScrolledWindow` with `set_policy` and a single child; no nested scrollers without a
  reason.
- Use `Gtk::Overlay` + `Gtk::Revealer` for notices.
- Do not block in a `draw`/`snapshot`/`measure` handler.
- Do not use `Gtk::Application::run()` twice; one application per process.
- No global widget pointers or singletons; inject dependencies.
- Dialogs use `Gtk::AlertDialog`/`Gtk::FileDialog`/custom `Gtk::Window` per the UI spec.
- Title bar switching follows the standard's unrealize sequence.

---

## 8. GLib/GIO

- Use `Glib::ustring` only at GTK boundaries; store UTF-8 `std::string` internally.
- Use `Glib::DateTime`/`std::chrono` for time; monotonic for durations.
- Use `Gio::File`/`Glib::file_get_contents` with size limits; do not read an untrusted file
  fully without a cap.
- Use `Gio::FileMonitor` for theme/config changes; debounce.
- Use `Gio::Subprocess` with argv, `Gio::SubprocessFlags::STDOUT_PIPE` etc., and a timeout.
- Use `Gio::SocketService` for the single-instance socket; check peer credentials.
- Use `Gio::DBus` for KWin/portal; probe names and methods; no sync call on the UI thread
  without a timeout.
- Use `GNotification` for notices to the desktop.
- Use `GChecksum` for SHA-256/MD5 where possible; OpenSSL only where needed.
- `GResource` for every bundled asset; never load from the source tree.

---

## 9. Files and storage

- Paths from `Glib::get_user_config_dir`, `get_user_data_dir`, `get_user_cache_dir`,
  `get_user_special_dir` and `XDG_RUNTIME_DIR`; injectable for tests.
- Create directories 0700, private files 0600.
- Atomic writes: temp in the same directory, write, `fsync`, `rename`; do not follow symlinks.
- Use prepared SQLite statements; one connection per process; WAL; busy timeout.
- Do not hold a transaction across UI work.
- Caches are bounded and evictable; user overrides are never evicted.
- Deleting user data requires explicit confirmation and a recovery path.

---

## 10. Networking

- libsoup 3, HTTPS, system trust store, timeouts, size caps, host allowlist.
- Never disable certificate verification, even in tests (use a local test CA).
- No credentials in URLs; headers or OAuth.
- No automatic retry storm; bounded backoff.
- Parse with limits; reject wrong content types.
- Keep the previous cache on failure.
- No remote code/templates/plugins.

---

## 11. Secrets

- libsecret only; no plaintext fallback.
- Never log, print, put in argv, or store in the database/settings.
- Clear temporary buffers where practical.
- A secret is read at the point of use, not held for the process lifetime.
- Removing a connection removes the secret.

---

## 12. External commands

- One `LaunchPlan`/`ProcessRunner` path. No `system`, `popen`, shell.
- Program allowlist per source. Flatpak IDs validated.
- Environment allowlist. Working directory validated.
- Timeout and bounded output.
- Treat exit status/signal/stderr as part of the result.
- A missing program is a user-facing error, not a crash.
- Never run a game/tool as root.

---

## 13. Logging

- `g_message`/`g_warning`/`g_critical` with `LGL_GAMES_LAUNCHER` log domain.
- One line per operation: what, target (redacted), result, bounded error.
- No secrets, tokens, cookies, ROM keys, save contents or full personal paths.
- A user-visible message is short; details go to the log.
- Diagnostics are opt-in, previewed and redacted.

---

## 14. Tests

- Every core module has a `g_test` executable and a CTest entry.
- Every bug fix has a regression test.
- Tests are deterministic, isolated and use temp XDG roots.
- Security boundaries have malicious-input tests.
- UI tests that need a display skip with 77.
- Fuzz targets for parsers and archives.
- No real ROMs/keys/saves/credentials in fixtures.
- A test that cannot run says why; it never silently passes.

---

## 15. Static analysis

- `clang-tidy`: modernize, bugprone, performance, readability, cppcoreguidelines. Suppressions
  are project-wide and documented with a reason.
- `cppcheck` with a checked-in suppressions file.
- CodeQL C++.
- ASan + UBSan; TSan where practical.
- No new warning, even if the rule is not enabled.
- Do not suppress a finding to get a green build; fix it or document why it is a false
  positive.

---

## 16. Documentation and comments

- Comment intent, invariants and trust boundaries, not syntax.
- Every public type/function has a short purpose, ownership, thread affinity and failure
  behaviour.
- Security-relevant code references the `SEC-*` rule it implements.
- A non-obvious workaround includes a link or a test name.
- Do not leave commented-out code; use Git history.

---

## 17. Review checklist

- [ ] Ownership and lifetime are obvious.
- [ ] Thread affinity is documented; no widget access off the main thread.
- [ ] Errors are handled; no ignored result.
- [ ] Inputs are validated and bounded.
- [ ] No shell, no secret, no symlink follow.
- [ ] GTK standard components/colours used.
- [ ] Tests added, including a failure case.
- [ ] Static analysis/sanitizers pass.
- [ ] Docs/feature matrix updated.
