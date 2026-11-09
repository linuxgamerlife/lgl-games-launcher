#pragma once

#include "core/flags_store.h"
#include "core/steam.h"

#include <string>
#include <vector>

namespace gameslauncher {

struct Game {
  SteamGame steam;
  GameFlags flags;
};

struct LibraryFilter {
  std::string search;
  std::string sort = "title";
  bool favorites_only = false;
  bool show_hidden = false;
  bool installed_only = false;
};

// Merges Steam records with local flags and applies the current view.
[[nodiscard]] std::vector<Game> load_games(const SteamScanResult& scan, FlagsStore& flags);
[[nodiscard]] std::vector<Game> filter_games(const std::vector<Game>& games,
                                             const LibraryFilter& filter);
[[nodiscard]] std::string normalise_title(const std::string& value);

} // namespace gameslauncher
