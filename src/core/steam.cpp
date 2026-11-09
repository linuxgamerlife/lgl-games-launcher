#include "core/steam.h"

#include "core/valve_key_values.h"

#include <glib.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace gameslauncher {
namespace {

namespace fs = std::filesystem;

constexpr std::uintmax_t kMaxManifestBytes = 8 * 1024 * 1024;
constexpr int kMaxManifests = 20000;

std::optional<std::string> read_file(const fs::path& path, std::string* error,
                                   std::uintmax_t max_bytes = kMaxManifestBytes) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > max_bytes) {
    if (error != nullptr) {
      *error = "file is missing or too large";
    }
    return std::nullopt;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error != nullptr) {
      *error = "could not open file";
    }
    return std::nullopt;
  }
  std::string contents(static_cast<std::size_t>(size), '\0');
  input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!input && !input.eof()) {
    if (error != nullptr) {
      *error = "could not read file";
    }
    return std::nullopt;
  }
  return contents;
}

std::string canonical_or_clean(const fs::path& path) {
  std::error_code ec;
  const fs::path canonical = fs::weakly_canonical(path, ec);
  return (ec ? path : canonical).string();
}

bool parse_manifest(const fs::path& manifest, const std::string& library_path,
                    SteamGame* game, std::string* error) {
  std::string read_error;
  const auto contents = read_file(manifest, &read_error);
  if (!contents) {
    if (error != nullptr) {
      *error = read_error;
    }
    return false;
  }
  std::string parse_error;
  const auto parsed = vdf::parse(*contents, &parse_error);
  if (!parsed) {
    if (error != nullptr) {
      *error = parse_error;
    }
    return false;
  }
  const vdf::Node* app = parsed->child("AppState");
  if (app == nullptr) {
    if (error != nullptr) {
      *error = "missing AppState";
    }
    return false;
  }
  const std::string app_id = app->string_value("appid");
  const std::string name = app->string_value("name");
  const std::string install_dir = app->string_value("installdir");
  if (app_id.empty() || name.empty() || install_dir.empty() ||
      !std::all_of(app_id.begin(), app_id.end(), [](unsigned char c) { return std::isdigit(c); })) {
    if (error != nullptr) {
      *error = "missing app id, name or install directory";
    }
    return false;
  }
  const long long flags = app->integer_value("StateFlags", 0);
  if (flags != 0 && (flags & 4) == 0) {
    if (error != nullptr) {
      *error = "not fully installed";
    }
    return false;
  }
  const fs::path install_path = fs::path(library_path) / "steamapps" / "common" / install_dir;
  game->app_id = app_id;
  game->name = name;
  game->install_dir = install_dir;
  game->install_path = install_path.string();
  game->library_path = library_path;
  game->manifest_path = manifest.string();
  game->last_played = app->integer_value("LastPlayed", 0);
  game->playtime_minutes = static_cast<int>(app->integer_value("Playtime", 0));
  game->size_on_disk = app->integer_value("SizeOnDisk", 0);
  std::error_code ec;
  game->installed = fs::is_directory(install_path, ec);
  return true;
}

std::vector<std::string> library_paths(const std::string& root, std::vector<std::string>* warnings) {
  std::vector<std::string> paths{root};
  fs::path library_file = fs::path(root) / "config" / "libraryfolders.vdf";
  if (!fs::exists(library_file)) {
    library_file = fs::path(root) / "steamapps" / "libraryfolders.vdf";
  }
  std::string error;
  const auto contents = read_file(library_file, &error);
  if (!contents) {
    if (warnings != nullptr) {
      warnings->push_back("Could not read " + library_file.string() + ": " + error);
    }
    return paths;
  }
  const auto parsed = vdf::parse(*contents, &error);
  if (!parsed) {
    if (warnings != nullptr) {
      warnings->push_back("Could not parse " + library_file.string() + ": " + error);
    }
    return paths;
  }
  const vdf::Node* folders = parsed->child("libraryfolders");
  if (folders == nullptr) {
    folders = &*parsed;
  }
  for (const auto& [key, child] : folders->children) {
    if (child.is_object) {
      const std::string path = child.string_value("path");
      if (!path.empty()) {
        paths.push_back(path);
      }
    } else if (!key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) {
                 return std::isdigit(c);
               }) &&
               !child.value.empty() && child.value.front() == '/') {
      paths.push_back(child.value);
    }
  }
  return paths;
}

std::string find_named(const fs::path& directory, const std::vector<std::string>& stems,
                       const std::vector<std::string>& extensions = {".jpg", ".png", ".webp"}) {
  std::error_code ec;
  if (!fs::is_directory(directory, ec)) {
    return {};
  }
  int inspected = 0;
  for (fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
       it != end && !ec && inspected < 2000; it.increment(ec), ++inspected) {
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const std::string filename = it->path().filename().string();
    for (const std::string& stem : stems) {
      for (const std::string& extension : extensions) {
        if (filename == stem + extension) {
          return it->path().string();
        }
      }
    }
  }
  return {};
}

std::string find_grid_file(const std::string& root, const std::string& app_id,
                           bool allow_hero_or_logo, bool hero, bool logo) {
  std::error_code ec;
  const fs::path userdata = fs::path(root) / "userdata";
  if (!fs::is_directory(userdata, ec)) {
    return {};
  }
  int users = 0;
  for (fs::directory_iterator user(userdata, fs::directory_options::skip_permission_denied, ec), end;
       user != end && !ec && users < 64; user.increment(ec), ++users) {
    if (!user->is_directory(ec)) {
      continue;
    }
    const fs::path grid = user->path() / "config" / "grid";
    std::error_code grid_ec;
    if (!fs::is_directory(grid, grid_ec)) {
      continue;
    }
    int inspected = 0;
    for (fs::directory_iterator file(grid, fs::directory_options::skip_permission_denied, grid_ec), fend;
         file != fend && !grid_ec && inspected < 4000; file.increment(grid_ec), ++inspected) {
      if (!file->is_regular_file(grid_ec)) {
        continue;
      }
      const std::string filename = file->path().filename().string();
      if (filename.rfind(app_id, 0) != 0) {
        continue;
      }
      const bool is_hero = filename.find("_hero.") != std::string::npos;
      const bool is_logo = filename.find("_logo.") != std::string::npos;
      if (hero && !is_hero) {
        continue;
      }
      if (logo && !is_logo) {
        continue;
      }
      if (!allow_hero_or_logo && (is_hero || is_logo)) {
        continue;
      }
      const std::string extension = file->path().extension().string();
      if (extension == ".jpg" || extension == ".png" || extension == ".webp" ||
          extension == ".jpeg") {
        return file->path().string();
      }
    }
  }
  return {};
}

std::string find_cache_file(const std::string& root, const std::string& app_id,
                            const std::vector<std::string>& stems) {
  const fs::path cache = fs::path(root) / "appcache" / "librarycache";
  const fs::path app_cache = cache / app_id;
  for (const std::string& stem : stems) {
    const std::string found = find_named(app_cache, {stem});
    if (!found.empty()) {
      return found;
    }
    // STEAM_SERVICE_ASSETS: Steam stores many library assets one level down
    // in an opaque hash directory (appid/<hash>/library_capsule.jpg).
    std::error_code directory_error;
    if (fs::is_directory(app_cache, directory_error)) {
      int subdirectories = 0;
      for (fs::directory_iterator child(
               app_cache, fs::directory_options::skip_permission_denied, directory_error),
           end;
           child != end && !directory_error && subdirectories < 16;
           child.increment(directory_error), ++subdirectories) {
        if (!child->is_directory(directory_error)) {
          continue;
        }
        const std::string nested = find_named(child->path(), {stem});
        if (!nested.empty()) {
          return nested;
        }
      }
    }
    for (const char* extension : {".jpg", ".png", ".webp", ".jpeg"}) {
      const fs::path candidate = cache / (app_id + "_" + stem + extension);
      std::error_code ec;
      if (fs::is_regular_file(candidate, ec)) {
        return candidate.string();
      }
    }
  }
  return {};
}

void collect_playtime(const vdf::Node& node, std::map<std::string, int>* playtime) {
  const vdf::Node* apps = node.child("apps");
  if (apps != nullptr && apps->is_object) {
    for (const auto& [app_id, record] : apps->children) {
      if (!record.is_object) {
        continue;
      }
      const long long minutes = record.integer_value("Playtime", -1);
      if (minutes >= 0) {
        (*playtime)[app_id] = std::max((*playtime)[app_id], static_cast<int>(minutes));
      }
    }
  }
  for (const auto& [key, child] : node.children) {
    if (child.is_object) {
      collect_playtime(child, playtime);
    }
  }
}

std::string steam_id_for_root(const std::string& root) {
  std::error_code ec;
  const fs::path login = fs::path(root) / "config" / "loginusers.vdf";
  std::string error;
  const auto contents = read_file(login, &error);
  if (!contents) {
    return {};
  }
  std::string parse_error;
  const auto parsed = vdf::parse(*contents, &parse_error);
  if (!parsed) {
    return {};
  }
  const vdf::Node* users = parsed->child("users");
  if (users == nullptr || !users->is_object) {
    return {};
  }
  std::string first;
  for (const auto& [id, user] : users->children) {
    if (!user.is_object || id.size() < 10 ||
        !std::all_of(id.begin(), id.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
      continue;
    }
    if (first.empty()) {
      first = id;
    }
    if (user.string_value("MostRecent") == "1") {
      return id;
    }
  }
  return first;
}

std::set<std::string> owned_app_ids(const std::string& root, const std::string& steam_id) {
  std::set<std::string> result;
  std::error_code ec;
  std::string user = steam_id;
  const fs::path userdata = fs::path(root) / "userdata";
  if (!user.empty() && !fs::is_directory(userdata / user, ec)) {
    // loginusers.vdf exposes SteamID64, while userdata uses the account id.
    user.clear();
  }
  if (user.empty() && fs::is_directory(userdata, ec)) {
    for (fs::directory_iterator it(userdata, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
      const std::string name = it->path().filename().string();
      if (it->is_directory(ec) && !name.empty() &&
          std::all_of(name.begin(), name.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
          })) {
        user = name;
        break;
      }
    }
  }
  if (user.empty()) {
    return result;
  }
  const fs::path cache = userdata / user / "config" / "librarycache";
  if (!fs::is_directory(cache, ec)) {
    return result;
  }
  int inspected = 0;
  for (fs::directory_iterator it(cache, fs::directory_options::skip_permission_denied, ec), end;
       it != end && !ec && inspected < 10000; it.increment(ec), ++inspected) {
    if (!it->is_regular_file(ec) || it->path().extension() != ".json") {
      continue;
    }
    const std::string app_id = it->path().stem().string();
    if (!app_id.empty() && app_id.size() <= 32 &&
        std::all_of(app_id.begin(), app_id.end(), [](unsigned char ch) {
          return std::isdigit(ch) != 0;
        })) {
      result.insert(app_id);
    }
  }
  return result;
}

// appinfo.vdf is a binary record store. Its common fields use the key indexes
// 4 (name) and 5 (type); the record's app id precedes the name field. This
// reads those fields only, bounds every string, and ignores anything it
// cannot associate with an owned app id.
void collect_local_app_ids(const vdf::Node& node, std::set<std::string>* ids) {
  const vdf::Node* apps = node.child("apps");
  if (apps != nullptr && apps->is_object) {
    for (const auto& [key, child] : apps->children) {
      if (!child.is_object || key.size() > 32 ||
          !std::all_of(key.begin(), key.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
          })) {
        continue;
      }
      ids->insert(key);
    }
  }
  for (const auto& [key, child] : node.children) {
    if (child.is_object) {
      collect_local_app_ids(child, ids);
    }
  }
}

std::set<std::string> local_app_ids(const std::string& root, const std::string& steam_id) {
  std::set<std::string> result;
  std::error_code ec;
  std::string user = steam_id;
  const fs::path userdata = fs::path(root) / "userdata";
  if (!user.empty() && !fs::is_directory(userdata / user, ec)) {
    user.clear();
  }
  if (user.empty() && fs::is_directory(userdata, ec)) {
    for (fs::directory_iterator it(userdata, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
      const std::string name = it->path().filename().string();
      if (it->is_directory(ec) && !name.empty() &&
          std::all_of(name.begin(), name.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
          })) {
        user = name;
        break;
      }
    }
  }
  if (user.empty()) {
    return result;
  }
  std::string error;
  const auto contents = read_file(userdata / user / "config" / "localconfig.vdf", &error,
                                  64 * 1024 * 1024);
  if (!contents) {
    return result;
  }
  const auto parsed = vdf::parse(*contents, &error);
  if (!parsed) {
    return result;
  }
  collect_local_app_ids(*parsed, &result);
  return result;
}

std::map<std::string, std::string> appinfo_names(const std::string& root,
                                                 const std::set<std::string>& wanted) {
  std::map<std::string, std::string> result;
  if (wanted.empty()) {
    return result;
  }
  std::string error;
  const auto contents = read_file(fs::path(root) / "appcache" / "appinfo.vdf", &error,
                                  64 * 1024 * 1024);
  if (!contents) {
    return result;
  }
  const std::string& data = *contents;
  if (data.size() < 8 || static_cast<unsigned char>(data[0]) != 0x29 ||
      static_cast<unsigned char>(data[1]) != 0x44 ||
      static_cast<unsigned char>(data[2]) != 0x56 ||
      static_cast<unsigned char>(data[3]) != 0x07) {
    return result;
  }
  const std::string name_marker("\x01\x04\x00\x00\x00", 5);
  const std::string type_marker("\x01\x05\x00\x00\x00", 5);
  std::size_t pos = 0;
  while ((pos = data.find(name_marker, pos)) != std::string::npos) {
    const std::size_t name_start = pos + name_marker.size();
    const void* null = std::memchr(data.data() + name_start, '\0', data.size() - name_start);
    if (null == nullptr) {
      break;
    }
    const std::size_t name_end = static_cast<const char*>(null) - data.data();
    const std::size_t name_length = name_end - name_start;
    if (name_length == 0 || name_length > 256) {
      pos = name_start;
      continue;
    }
    std::string app_id;
    const std::size_t back_limit = pos > 192 ? pos - 192 : 0;
    for (std::size_t back = back_limit; back + 4 <= pos; ++back) {
      const auto value = static_cast<std::uint32_t>(
          static_cast<unsigned char>(data[back]) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(data[back + 1])) << 8) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(data[back + 2])) << 16) |
          (static_cast<std::uint32_t>(static_cast<unsigned char>(data[back + 3])) << 24));
      const std::string candidate = std::to_string(value);
      if (wanted.find(candidate) != wanted.end()) {
        app_id = candidate;
      }
    }
    if (!app_id.empty()) {
      std::string type;
      const std::size_t type_search_end = std::min(data.size(), name_end + 96);
      const std::size_t type_pos = data.find(type_marker, name_end);
      if (type_pos != std::string::npos && type_pos <= type_search_end) {
        const std::size_t type_start = type_pos + type_marker.size();
        const void* type_null = std::memchr(data.data() + type_start, '\0',
                                            data.size() - type_start);
        if (type_null != nullptr) {
          const std::size_t type_end = static_cast<const char*>(type_null) - data.data();
          if (type_end - type_start <= 64) {
            type = data.substr(type_start, type_end - type_start);
          }
        }
      }
      const std::string candidate_name = data.substr(name_start, name_length);
      const bool include = (type.empty() || type == "Game" || type == "Demo" ||
                            type == "Beta" || type == "Playtest") &&
                           candidate_name.find(" EULA") == std::string::npos &&
                           result.find(app_id) == result.end();
      if (include) {
        result[app_id] = candidate_name;
      }
    }
    pos = name_end;
  }
  return result;
}

std::map<std::string, int> playtime_for_root(const std::string& root) {
  std::map<std::string, int> result;
  std::error_code ec;
  const fs::path userdata = fs::path(root) / "userdata";
  if (!fs::is_directory(userdata, ec)) {
    return result;
  }
  int users = 0;
  for (fs::directory_iterator user(userdata, fs::directory_options::skip_permission_denied, ec), end;
       user != end && !ec && users < 64; user.increment(ec), ++users) {
    if (!user->is_directory(ec)) {
      continue;
    }
    const fs::path config = user->path() / "config" / "localconfig.vdf";
    std::string error;
    const auto contents = read_file(config, &error);
    if (!contents) {
      continue;
    }
    std::string parse_error;
    const auto parsed = vdf::parse(*contents, &parse_error);
    if (!parsed) {
      continue;
    }
    collect_playtime(*parsed, &result);
  }
  return result;
}

} // namespace

bool SteamScanner::is_tool_title(const std::string& name) {
  std::string normalized;
  normalized.reserve(name.size());
  for (unsigned char ch : name) {
    normalized.push_back(static_cast<char>(std::tolower(ch)));
  }
  const auto starts = [&normalized](std::string_view prefix) {
    return normalized.rfind(prefix, 0) == 0;
  };
  return starts("proton ") || starts("steam linux runtime") ||
         starts("steamworks common redistributables") || starts("steam runtime") ||
         starts("steamvr");
}

std::vector<std::string> SteamScanner::discover_roots() {
  const std::string home = Glib::get_home_dir();
  const std::string xdg_data = Glib::get_user_data_dir();
  std::vector<std::string> candidates = {
      home + "/.local/share/Steam",
      home + "/.steam/steam",
      home + "/.var/app/com.valvesoftware.Steam/data/Steam",
      home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
      xdg_data + "/Steam",
  };
  std::vector<std::string> roots;
  std::set<std::string> seen;
  for (const std::string& candidate : candidates) {
    std::error_code ec;
    if (!fs::is_directory(fs::path(candidate) / "steamapps", ec)) {
      continue;
    }
    const std::string root = canonical_or_clean(candidate);
    if (seen.insert(root).second) {
      roots.push_back(root);
    }
  }
  return roots;
}

SteamScanResult SteamScanner::scan() {
  SteamScanResult result;
  result.roots = discover_roots();
  result.flatpak = std::any_of(result.roots.begin(), result.roots.end(), [](const std::string& root) {
    return root.find("/com.valvesoftware.Steam/") != std::string::npos;
  });
  if (result.roots.empty()) {
    result.warnings.push_back("No Steam library was found.");
    return result;
  }

  std::set<std::string> seen_apps;
  for (const std::string& root : result.roots) {
    if (result.steam_id.empty()) {
      result.steam_id = steam_id_for_root(root);
    }
    const std::map<std::string, int> playtime = playtime_for_root(root);
    for (const std::string& library : library_paths(root, &result.warnings)) {
      std::error_code ec;
      const fs::path apps = fs::path(library) / "steamapps";
      if (!fs::is_directory(apps, ec)) {
        continue;
      }
      int count = 0;
      for (fs::directory_iterator it(apps, fs::directory_options::skip_permission_denied, ec), end;
           it != end && !ec && count < kMaxManifests; it.increment(ec), ++count) {
        if (!it->is_regular_file(ec)) {
          continue;
        }
        const std::string filename = it->path().filename().string();
        if (filename.rfind("appmanifest_", 0) != 0 || it->path().extension() != ".acf") {
          continue;
        }
        SteamGame game;
        std::string error;
        if (!parse_manifest(it->path(), library, &game, &error)) {
          continue;
        }
        if (is_tool_title(game.name) || !seen_apps.insert(game.app_id).second) {
          continue;
        }
        const auto minutes = playtime.find(game.app_id);
        if (minutes != playtime.end()) {
          game.playtime_minutes = std::max(game.playtime_minutes, minutes->second);
        }
        game.cover_path = find_cover(root, game.app_id);
        game.hero_path = find_hero(root, game.app_id);
        game.logo_path = find_logo(root, game.app_id);
        result.games.push_back(std::move(game));
      }
    }

    std::set<std::string> owned = owned_app_ids(root, result.steam_id);
    const std::set<std::string> local = local_app_ids(root, result.steam_id);
    owned.insert(local.begin(), local.end());
    if (!owned.empty()) {
      const std::map<std::string, std::string> names = appinfo_names(root, owned);
      for (const std::string& app_id : owned) {
        if (seen_apps.find(app_id) != seen_apps.end()) {
          continue;
        }
        const auto named = names.find(app_id);
        if (named == names.end() || named->second.empty()) {
          continue;
        }
        SteamGame game;
        game.app_id = app_id;
        game.name = named->second;
        game.library_path = root;
        game.installed = false;
        const auto minutes = playtime.find(app_id);
        if (minutes != playtime.end()) {
          game.playtime_minutes = minutes->second;
        }
        game.cover_path = find_cover(root, app_id);
        game.hero_path = find_hero(root, app_id);
        game.logo_path = find_logo(root, app_id);
        if (seen_apps.insert(app_id).second) {
          result.games.push_back(std::move(game));
        }
      }
    }
  }
  std::sort(result.games.begin(), result.games.end(), [](const SteamGame& first, const SteamGame& second) {
    std::string a = first.name;
    std::string b = second.name;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return std::tolower(c); });
    return a < b;
  });
  return result;
}

std::string SteamScanner::find_cover(const std::string& root, const std::string& app_id) {
  std::string found = find_grid_file(root, app_id, false, false, false);
  if (!found.empty()) {
    return found;
  }
  return find_cache_file(root, app_id, {"library_600x900", "library_600x900_2x",
                                        "library_capsule", "header", "library_header",
                                        "library_hero"});
}

std::string SteamScanner::find_hero(const std::string& root, const std::string& app_id) {
  std::string found = find_grid_file(root, app_id, true, true, false);
  if (!found.empty()) {
    return found;
  }
  return find_cache_file(root, app_id, {"library_hero", "header"});
}

std::string SteamScanner::find_logo(const std::string& root, const std::string& app_id) {
  std::string found = find_grid_file(root, app_id, true, false, true);
  if (!found.empty()) {
    return found;
  }
  return find_cache_file(root, app_id, {"logo"});
}

} // namespace gameslauncher
