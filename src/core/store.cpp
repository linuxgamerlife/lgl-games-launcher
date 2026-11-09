#include "core/store.h"

#include <glib.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <utility>

namespace gameslauncher {
namespace {

namespace fs = std::filesystem;

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string slug(const std::string& value) {
  std::string result;
  for (unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      result.push_back(static_cast<char>(std::tolower(ch)));
    } else if (!result.empty() && result.back() != '-') {
      result.push_back('-');
    }
  }
  while (!result.empty() && result.back() == '-') {
    result.pop_back();
  }
  return result;
}

bool has_file_with_extension(const fs::path& dir, const std::string& extension) {
  std::error_code error;
  for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    if (it->is_regular_file(error) && lower(it->path().extension().string()) == extension) {
      return true;
    }
  }
  return false;
}

std::vector<fs::path> find_named_files(const fs::path& root, const std::string& name,
                                       int max_depth) {
  std::vector<fs::path> found;
  std::error_code error;
  if (!fs::is_directory(root, error)) {
    return found;
  }
  for (fs::recursive_directory_iterator it(
           root, fs::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    if (it.depth() >= max_depth) {
      it.disable_recursion_pending();
    }
    std::error_code entry_error;
    if (it->is_regular_file(entry_error) &&
        lower(it->path().filename().string()) == lower(name)) {
      found.push_back(it->path());
    }
  }
  return found;
}

std::vector<std::string> registry_values(const std::string& registry_path,
                                         const std::string& section_filter,
                                         const std::vector<std::string>& names) {
  std::vector<std::string> values;
  std::ifstream file(registry_path);
  if (!file) {
    return values;
  }
  bool in_section = false;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.front() == '[') {
      in_section = line.find(section_filter) != std::string::npos;
      continue;
    }
    if (!in_section) {
      continue;
    }
    for (const std::string& name : names) {
      const std::string prefix = "\"" + name + "\"=";
      if (line.rfind(prefix, 0) != 0) {
        continue;
      }
      const std::size_t first = line.find('"', prefix.size());
      const std::size_t last = line.find('"', first + 1);
      if (first != std::string::npos && last != std::string::npos) {
        std::string value = line.substr(first + 1, last - first - 1);
        std::string unescaped;
        unescaped.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index) {
          if (value[index] == '\\' && index + 1 < value.size() &&
              value[index + 1] == '\\') {
            unescaped.push_back('\\');
            ++index;
          } else {
            unescaped.push_back(value[index]);
          }
        }
        values.push_back(std::move(unescaped));
      }
    }
  }
  return values;
}

std::vector<fs::path> store_game_dirs(const fs::path& root) {
  std::vector<fs::path> dirs;
  std::error_code error;
  if (!fs::is_directory(root, error)) {
    return dirs;
  }
  for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    std::error_code entry_error;
    if (!it->is_directory(entry_error) || fs::is_empty(it->path(), entry_error)) {
      continue;
    }
    if (has_file_with_extension(it->path(), ".exe") ||
        fs::exists(it->path() / "uplay_install.state", entry_error)) {
      dirs.push_back(it->path());
    }
  }
  return dirs;
}

std::string json_string(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) ||
      json_node_get_value_type(node) != G_TYPE_STRING) {
    return {};
  }
  const char* value = json_node_get_string(node);
  return value != nullptr ? value : "";
}

std::string prefix_of(const FaugusGame& entry) {
  if (!entry.prefix.empty()) {
    return entry.prefix;
  }
  if (!entry.path.empty()) {
    return fs::path(entry.path).parent_path().parent_path().string();
  }
  return {};
}

std::string largest_executable(const fs::path& dir) {
  std::string best;
  std::uintmax_t best_size = 0;
  std::error_code error;
  if (!fs::is_directory(dir, error)) {
    return best;
  }
  for (fs::recursive_directory_iterator it(
           dir, fs::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    if (it.depth() >= 4) {
      it.disable_recursion_pending();
    }
    std::error_code entry_error;
    if (!it->is_regular_file(entry_error) ||
        lower(it->path().extension().string()) != ".exe") {
      continue;
    }
    const std::string name = lower(it->path().filename().string());
    if (name.rfind("unins", 0) == 0 || name.rfind("vcredist", 0) == 0 ||
        name.rfind("dxsetup", 0) == 0 || name.rfind("setup", 0) == 0) {
      continue;
    }
    const std::uintmax_t size = it->file_size(entry_error);
    if (!entry_error && size > best_size) {
      best_size = size;
      best = it->path().string();
    }
  }
  return best;
}

void append_unique(std::vector<StoreGame>& games, StoreGame game) {
  const bool exists = std::any_of(games.begin(), games.end(), [&game](const StoreGame& other) {
    return other.install_dir == game.install_dir;
  });
  if (!exists) {
    games.push_back(std::move(game));
  }
}

std::vector<StoreGame> scan_ubisoft(const FaugusGame& entry, const std::string& prefix) {
  std::vector<StoreGame> games;
  std::vector<fs::path> dirs;
  for (const fs::path& root :
       {fs::path(prefix) / "drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games",
        fs::path(prefix) / "drive_c/Program Files/Ubisoft/Ubisoft Game Launcher/games"}) {
    const auto found = store_game_dirs(root);
    dirs.insert(dirs.end(), found.begin(), found.end());
  }
  // Ubisoft can install a game anywhere under the prefix. Its install state
  // file sits in the game directory.
  for (const fs::path& state :
       find_named_files(fs::path(prefix) / "drive_c", "uplay_install.state", 8)) {
    dirs.push_back(state.parent_path());
  }
  for (const std::string& value :
       registry_values(Glib::build_filename(prefix, "user.reg"), "Ubisoft\\\\Launcher",
                       {"InstallDir", "Install Dir"})) {
    const std::string path = StoreScanner::windows_to_unix(prefix, value);
    std::error_code error;
    if (!path.empty() && fs::is_directory(path, error)) {
      dirs.emplace_back(path);
    }
  }

  for (const fs::path& dir : dirs) {
    StoreGame game;
    game.title = dir.filename().string();
    if (game.title.empty()) {
      continue;
    }
    game.id = "ubisoft:" + slug(game.title);
    game.install_dir = dir.string();
    game.prefix = prefix;
    game.launcher_gameid = entry.gameid;
    game.executable = largest_executable(dir);
    game.runner = entry.runner;
    game.launch_arguments = entry.launch_arguments;
    game.store = StoreKind::Ubisoft;
    game.source = "Ubisoft";
    append_unique(games, std::move(game));
  }
  return games;
}

std::vector<StoreGame> scan_ea(const FaugusGame& entry, const std::string& prefix) {
  std::vector<StoreGame> games;
  std::vector<fs::path> dirs;
  for (const fs::path& root :
       {fs::path(prefix) / "drive_c/Program Files/EA Games",
        fs::path(prefix) / "drive_c/Program Files (x86)/EA Games"}) {
    const auto found = store_game_dirs(root);
    dirs.insert(dirs.end(), found.begin(), found.end());
  }
  for (const std::string& value :
       registry_values(Glib::build_filename(prefix, "user.reg"),
                       "Electronic Arts\\\\EA Games", {"Install Dir", "InstallLocation"})) {
    const std::string path = StoreScanner::windows_to_unix(prefix, value);
    std::error_code error;
    if (!path.empty() && fs::is_directory(path, error)) {
      dirs.emplace_back(path);
    }
  }

  for (const fs::path& dir : dirs) {
    StoreGame game;
    game.title = dir.filename().string();
    if (game.title.empty()) {
      continue;
    }
    game.id = "ea:" + slug(game.title);
    game.install_dir = dir.string();
    game.prefix = prefix;
    game.launcher_gameid = entry.gameid;
    game.executable = largest_executable(dir);
    game.runner = entry.runner;
    game.launch_arguments = entry.launch_arguments;
    game.store = StoreKind::EA;
    game.source = "EA";
    append_unique(games, std::move(game));
  }
  return games;
}

std::vector<StoreGame> scan_epic(const FaugusGame& entry, const std::string& prefix) {
  std::vector<StoreGame> games;
  const fs::path manifests =
      fs::path(prefix) / "drive_c/ProgramData/Epic/EpicGamesLauncher/Data/Manifests";
  std::error_code error;
  if (fs::is_directory(manifests, error)) {
    for (fs::directory_iterator it(manifests, fs::directory_options::skip_permission_denied,
                                   error),
         end;
         !error && it != end; it.increment(error)) {
      if (!it->is_regular_file(error) || lower(it->path().extension().string()) != ".item") {
        continue;
      }
      JsonParser* parser = json_parser_new();
      if (!json_parser_load_from_file(parser, it->path().c_str(), nullptr)) {
        g_object_unref(parser);
        continue;
      }
      JsonNode* root = json_parser_get_root(parser);
      JsonObject* object =
          (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root)
                                                            : nullptr;
      StoreGame game;
      game.title = json_string(object, "DisplayName");
      const std::string app_name = json_string(object, "AppName");
      const std::string location = json_string(object, "InstallLocation");
      const std::string executable = json_string(object, "LaunchExecutable");
      if (!game.title.empty() && (!location.empty() || !app_name.empty())) {
        game.id = "epic:" + (app_name.empty() ? slug(game.title) : app_name);
        game.app_name = app_name;
        game.install_dir = StoreScanner::windows_to_unix(prefix, location);
        if (!executable.empty()) {
          const bool windows_absolute =
              executable.find(':') != std::string::npos || executable.rfind("\\\\", 0) == 0;
          game.executable = windows_absolute
                                ? StoreScanner::windows_to_unix(prefix, executable)
                                : (fs::path(game.install_dir) / executable).string();
        }
        if (game.executable.empty() || !fs::is_regular_file(game.executable)) {
          game.executable = largest_executable(game.install_dir);
        }
        game.runner = entry.runner;
        game.launch_arguments = entry.launch_arguments;
        game.prefix = prefix;
        game.launcher_gameid = entry.gameid;
        game.store = StoreKind::Epic;
        game.source = "Epic";
        games.push_back(std::move(game));
      }
      g_object_unref(parser);
    }
  }

  // Older launchers only write the compact LauncherInstalled.dat.
  const fs::path installed =
      fs::path(prefix) / "drive_c/ProgramData/Epic/UnrealEngineLauncher/LauncherInstalled.dat";
  if (fs::is_regular_file(installed, error)) {
    JsonParser* parser = json_parser_new();
    if (json_parser_load_from_file(parser, installed.c_str(), nullptr)) {
      JsonNode* root = json_parser_get_root(parser);
      JsonObject* object =
          (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root)
                                                            : nullptr;
      if (object != nullptr && json_object_has_member(object, "InstallationList")) {
        JsonArray* list = json_object_get_array_member(object, "InstallationList");
        for (guint index = 0; list != nullptr && index < json_array_get_length(list); ++index) {
          JsonObject* item = json_array_get_object_element(list, index);
          const std::string app_name = json_string(item, "AppName");
          const std::string location = json_string(item, "InstallLocation");
          if (app_name.empty()) {
            continue;
          }
          StoreGame game;
          game.title = !location.empty() ? fs::path(location).filename().string() : app_name;
          game.id = "epic:" + app_name;
          game.app_name = app_name;
          game.install_dir = StoreScanner::windows_to_unix(prefix, location);
          game.executable = largest_executable(game.install_dir);
          game.runner = entry.runner;
          game.launch_arguments = entry.launch_arguments;
          game.prefix = prefix;
          game.launcher_gameid = entry.gameid;
          game.store = StoreKind::Epic;
          game.source = "Epic";
          games.push_back(std::move(game));
        }
      }
    }
    g_object_unref(parser);
  }
  return games;
}

} // namespace

std::string StoreScanner::windows_to_unix(const std::string& prefix,
                                          const std::string& windows_path) {
  if (windows_path.empty()) {
    return {};
  }
  if (windows_path.front() == '/') {
    return windows_path;
  }
  std::string path = windows_path;
  std::replace(path.begin(), path.end(), '\\', '/');
  if (path.size() >= 2 && path[1] == ':') {
    const char drive = static_cast<char>(std::tolower(path[0]));
    const std::string rest = path.size() > 2 ? path.substr(2) : "";
    const std::string drive_link =
        Glib::build_filename(prefix, "dosdevices", std::string(1, drive) + ":");
    std::error_code error;
    const std::string target = fs::read_symlink(drive_link, error).string();
    if (!error) {
      const fs::path resolved =
          fs::path(target).is_absolute() ? fs::path(target)
                                         : fs::path(prefix) / target;
      return (resolved / rest).lexically_normal().string();
    }
    if (drive == 'c') {
      return (fs::path(prefix) / "drive_c" / rest).lexically_normal().string();
    }
  }
  return {};
}

StoreScanResult StoreScanner::scan(const FaugusScanResult& faugus) {
  StoreScanResult result;
  for (const FaugusGame& entry : faugus.games) {
    const std::string prefix = prefix_of(entry);
    if (prefix.empty()) {
      continue;
    }
    std::vector<StoreGame> found;
    if (entry.source == "Ubisoft") {
      found = scan_ubisoft(entry, prefix);
    } else if (entry.source == "EA") {
      found = scan_ea(entry, prefix);
    } else if (entry.source == "Epic") {
      found = scan_epic(entry, prefix);
    }
    result.games.insert(result.games.end(), std::make_move_iterator(found.begin()),
                        std::make_move_iterator(found.end()));
  }

  // Deduplicate by id, keeping the first prefix that produced it.
  std::vector<StoreGame> unique;
  for (StoreGame& game : result.games) {
    const bool exists = std::any_of(unique.begin(), unique.end(),
                                    [&game](const StoreGame& other) {
                                      return other.id == game.id;
                                    });
    if (!exists) {
      unique.push_back(std::move(game));
    }
  }
  result.games = std::move(unique);
  return result;
}

} // namespace gameslauncher
