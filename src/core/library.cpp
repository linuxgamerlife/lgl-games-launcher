#include "core/library.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace gameslauncher {
namespace {

bool source_matches(const Game& game, const std::string& source) {
  if (source.empty() || source == "all") {
    return true;
  }
  if (source == "steam") {
    return game.kind == GameKind::Steam;
  }
  if (source == "faugus") {
    return game.kind == GameKind::Faugus && game.launcher && game.installed;
  }
  if (source == "ubisoft" || source == "ea" || source == "epic") {
    if (game.kind != GameKind::Store) {
      return false;
    }
    std::string label = game.source;
    std::transform(label.begin(), label.end(), label.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return label == source;
  }
  std::string label = game.source;
  std::transform(label.begin(), label.end(), label.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return label == source;
}

} // namespace

std::string normalise_title(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  bool previous_space = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      result.push_back(static_cast<char>(std::tolower(ch)));
      previous_space = false;
    } else if (!previous_space && !result.empty()) {
      result.push_back(' ');
      previous_space = true;
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::vector<Game> load_games(const SteamScanResult& scan, const FaugusScanResult& faugus,
                             const StoreScanResult& stores, FlagsStore& flags) {
  std::vector<Game> games;
  games.reserve(scan.games.size() + faugus.games.size() + stores.games.size());
  for (const SteamGame& steam : scan.games) {
    Game game;
    game.app_id = steam.app_id;
    game.steam_app_id = steam.app_id;
    game.title = steam.name;
    game.source = "Steam";
    game.kind = GameKind::Steam;
    game.install_path = steam.install_path;
    game.library_path = steam.library_path;
    game.cover_path = steam.cover_path;
    game.hero_path = steam.hero_path;
    game.logo_path = steam.logo_path;
    game.last_played = steam.last_played;
    game.playtime_minutes = steam.playtime_minutes;
    game.size_on_disk = steam.size_on_disk;
    game.installed = steam.installed;
    game.flatpak =
        steam.library_path.find("/com.valvesoftware.Steam/") != std::string::npos;
    game.flags = flags.get(game.app_id);
    games.push_back(std::move(game));
  }
  for (const FaugusGame& entry : faugus.games) {
    Game game;
    game.app_id = "faugus:" + entry.gameid;
    game.faugus_gameid = entry.gameid;
    game.title = entry.title;
    game.source = entry.source;
    game.kind = GameKind::Faugus;
    game.install_path = entry.path;
    game.library_path = entry.prefix;
    game.cover_path = entry.cover_path;
    game.last_played = entry.last_played;
    game.playtime_minutes = entry.playtime_minutes;
    std::error_code error;
    game.installed = entry.path.empty() || std::filesystem::exists(entry.path, error);
    game.launcher = entry.source == "Ubisoft" || entry.source == "EA" ||
                    entry.source == "Epic";
    game.runner = entry.runner;
    game.launch_arguments = entry.launch_arguments;
    game.flags = flags.get(game.app_id);
    games.push_back(std::move(game));
  }
  for (const StoreGame& entry : stores.games) {
    Game game;
    game.app_id = "store:" + entry.id;
    game.store_launcher_gameid = entry.launcher_gameid;
    game.epic_app_name = entry.app_name;
    game.title = entry.title;
    game.source = entry.source;
    game.kind = GameKind::Store;
    game.install_path = entry.install_dir;
    game.library_path = entry.prefix;
    game.installed = true;
    game.executable = entry.executable;
    game.runner = entry.runner;
    game.launch_arguments = entry.launch_arguments;
    game.flags = flags.get(game.app_id);
    games.push_back(std::move(game));
  }
  return games;
}

std::vector<Game> filter_games(const std::vector<Game>& games, const LibraryFilter& filter) {
  const std::string query = normalise_title(filter.search);
  std::vector<Game> result;
  result.reserve(games.size());
  for (const Game& game : games) {
    if (game.flags.hidden && !filter.show_hidden) {
      continue;
    }
    if (!source_matches(game, filter.source)) {
      continue;
    }
    if (filter.favorites_only && !game.flags.favorite) {
      continue;
    }
    if (filter.installed_only && !game.installed) {
      continue;
    }
    if (!query.empty()) {
      const std::string title = normalise_title(game.title);
      const std::string tags = normalise_title(game.flags.tags);
      const std::string source = normalise_title(game.source);
      if (title.find(query) == std::string::npos && tags.find(query) == std::string::npos &&
          source.find(query) == std::string::npos &&
          game.app_id.find(query) == std::string::npos) {
        continue;
      }
    }
    result.push_back(game);
  }

  if (filter.sort == "recent") {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return a.last_played > b.last_played;
    });
  } else if (filter.sort == "playtime") {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return a.playtime_minutes > b.playtime_minutes;
    });
  } else {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return normalise_title(a.title) < normalise_title(b.title);
    });
  }
  return result;
}

} // namespace gameslauncher
