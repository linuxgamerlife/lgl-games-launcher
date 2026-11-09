#pragma once

#include <glibmm/keyfile.h>

#include <string>

namespace gameslauncher {

// Typed settings backed by settings.ini. Every setter saves immediately.
// A missing or malformed file falls back to defaults and never interrupts the
// user.
class Settings {
public:
  explicit Settings(std::string path = {});

  [[nodiscard]] const std::string& path() const { return m_path; }

  [[nodiscard]] bool system_title_bar() const;
  void set_system_title_bar(bool value);

  [[nodiscard]] std::string sort_mode() const;
  void set_sort_mode(const std::string& value);

  [[nodiscard]] bool show_hidden() const;
  void set_show_hidden(bool value);

  [[nodiscard]] bool installed_only() const;
  void set_installed_only(bool value);

  [[nodiscard]] int cover_size() const;
  void set_cover_size(int value);

  [[nodiscard]] int window_width() const;
  void set_window_width(int value);

  [[nodiscard]] int window_height() const;
  void set_window_height(int value);

  [[nodiscard]] std::string igdb_client_id() const;
  void set_igdb_client_id(const std::string& value);

  bool save();

private:
  void load();
  void ensure_loaded();

  std::string m_path;
  Glib::RefPtr<Glib::KeyFile> m_file;
  bool m_loaded = false;
};

} // namespace gameslauncher
