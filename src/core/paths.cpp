#include "core/paths.h"

#include <glib.h>
#include <glibmm/miscutils.h>

#include <sys/stat.h>

namespace gameslauncher {
namespace {

std::string join(const std::string& base, const std::string& name) {
  return Glib::build_filename(base, name);
}

} // namespace

std::string config_dir() {
  return join(Glib::get_user_config_dir(), "lgl-games-launcher");
}

std::string data_dir() {
  return join(Glib::get_user_data_dir(), "lgl-games-launcher");
}

std::string cache_dir() {
  return join(Glib::get_user_cache_dir(), "lgl-games-launcher");
}

std::string settings_path() { return join(config_dir(), "settings.ini"); }

std::string database_path() { return join(data_dir(), "library.sqlite3"); }

bool ensure_private_dir(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  if (g_mkdir_with_parents(path.c_str(), 0700) != 0) {
    g_warning("Could not create private directory %s", path.c_str());
    return false;
  }
  // The application must not depend on the umask for its private state.
  if (::chmod(path.c_str(), 0700) != 0) {
    g_warning("Could not set permissions on %s", path.c_str());
  }
  return true;
}

} // namespace gameslauncher
