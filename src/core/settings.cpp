#include "core/settings.h"

#include "core/paths.h"

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <sys/stat.h>

namespace gameslauncher {
namespace {

constexpr int kMinCover = 96;
constexpr int kMaxCover = 260;
constexpr int kMinWidth = 480;
constexpr int kMaxWidth = 16384;
constexpr int kMinHeight = 360;
constexpr int kMaxHeight = 16384;

std::string valid_sort(std::string value) {
  if (value != "recent" && value != "playtime") {
    return "title";
  }
  return value;
}

} // namespace

Settings::Settings(std::string path)
    : m_path(path.empty() ? settings_path() : std::move(path)),
      m_file(Glib::KeyFile::create()) {
  load();
}

void Settings::ensure_loaded() {
  if (!m_loaded) {
    load();
  }
}

void Settings::load() {
  m_loaded = true;
  if (g_file_test(m_path.c_str(), G_FILE_TEST_EXISTS)) {
    try {
      m_file->load_from_file(m_path);
    } catch (const Glib::Error& error) {
      g_warning("Could not read settings %s: %s", m_path.c_str(), error.what());
      m_file = Glib::KeyFile::create();
    }
  }
}

bool Settings::system_title_bar() const {
  try {
    return m_file->get_boolean("window", "systemTitleBar");
  } catch (const Glib::Error&) {
    return false;
  }
}

void Settings::set_system_title_bar(bool value) {
  ensure_loaded();
  m_file->set_boolean("window", "systemTitleBar", value);
  save();
}

std::string Settings::sort_mode() const {
  try {
    return valid_sort(m_file->get_string("library", "sort"));
  } catch (const Glib::Error&) {
    return "title";
  }
}

void Settings::set_sort_mode(const std::string& value) {
  ensure_loaded();
  m_file->set_string("library", "sort", valid_sort(value));
  save();
}

bool Settings::show_hidden() const {
  try {
    return m_file->get_boolean("library", "showHidden");
  } catch (const Glib::Error&) {
    return false;
  }
}

void Settings::set_show_hidden(bool value) {
  ensure_loaded();
  m_file->set_boolean("library", "showHidden", value);
  save();
}

int Settings::cover_size() const {
  try {
    return std::clamp(m_file->get_integer("library", "coverSize"), kMinCover, kMaxCover);
  } catch (const Glib::Error&) {
    return 160;
  }
}

void Settings::set_cover_size(int value) {
  ensure_loaded();
  m_file->set_integer("library", "coverSize", std::clamp(value, kMinCover, kMaxCover));
  save();
}

int Settings::window_width() const {
  try {
    return std::clamp(m_file->get_integer("window", "width"), kMinWidth, kMaxWidth);
  } catch (const Glib::Error&) {
    return 1280;
  }
}

void Settings::set_window_width(int value) {
  ensure_loaded();
  m_file->set_integer("window", "width", std::clamp(value, kMinWidth, kMaxWidth));
  save();
}

int Settings::window_height() const {
  try {
    return std::clamp(m_file->get_integer("window", "height"), kMinHeight, kMaxHeight);
  } catch (const Glib::Error&) {
    return 800;
  }
}

void Settings::set_window_height(int value) {
  ensure_loaded();
  m_file->set_integer("window", "height", std::clamp(value, kMinHeight, kMaxHeight));
  save();
}

std::string Settings::igdb_client_id() const {
  try {
    const std::string value = m_file->get_string("connections", "igdbClientId");
    if (value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
          return std::isalnum(ch) != 0;
        })) {
      return value;
    }
  } catch (const Glib::Error&) {
  }
  return {};
}

void Settings::set_igdb_client_id(const std::string& value) {
  ensure_loaded();
  std::string safe;
  for (unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      safe.push_back(static_cast<char>(ch));
    }
  }
  if (safe.size() > 64) {
    safe.resize(64);
  }
  m_file->set_string("connections", "igdbClientId", safe);
  save();
}

bool Settings::save() {
  const std::string directory = config_dir();
  if (!ensure_private_dir(directory)) {
    return false;
  }
  try {
    m_file->save_to_file(m_path);
  } catch (const Glib::Error& error) {
    g_warning("Could not save settings %s: %s", m_path.c_str(), error.what());
    return false;
  }
  ::chmod(m_path.c_str(), 0600);
  return true;
}

} // namespace gameslauncher
