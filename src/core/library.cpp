#include "core/library.h"

#include <algorithm>
#include <cctype>

namespace gameslauncher {

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

std::vector<Game> load_games(const SteamScanResult& scan, FlagsStore& flags) {
  std::vector<Game> games;
  games.reserve(scan.games.size());
  for (const SteamGame& steam : scan.games) {
    Game game;
    game.steam = steam;
    game.flags = flags.get(steam.app_id);
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
    if (filter.favorites_only && !game.flags.favorite) {
      continue;
    }
    if (!query.empty()) {
      const std::string title = normalise_title(game.steam.name);
      const std::string tags = normalise_title(game.flags.tags);
      if (title.find(query) == std::string::npos && tags.find(query) == std::string::npos &&
          game.steam.app_id.find(query) == std::string::npos) {
        continue;
      }
    }
    result.push_back(game);
  }

  if (filter.sort == "recent") {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return a.steam.last_played > b.steam.last_played;
    });
  } else if (filter.sort == "playtime") {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return a.steam.playtime_minutes > b.steam.playtime_minutes;
    });
  } else {
    std::stable_sort(result.begin(), result.end(), [](const Game& a, const Game& b) {
      return normalise_title(a.steam.name) < normalise_title(b.steam.name);
    });
  }
  return result;
}

} // namespace gameslauncher
