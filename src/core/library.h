#pragma once

#include "core/faugus.h"
#include "core/flags_store.h"
#include "core/steam.h"
#include "core/store.h"

#include <string>
#include <vector>

namespace gameslauncher {

enum class GameKind {
  Steam,
  Faugus,
  Store,
};

// A single library entry. Steam and Faugus records are normalised into the same
// shape so the grid and details page stay source-agnostic.
struct Game {
  std::string app_id;        // Stable key: Steam app id, or "faugus:<gameid>".
  std::string steam_app_id;  // Numeric Steam id; empty for Faugus entries.
  std::string faugus_gameid;
  std::string store_launcher_gameid;
  std::string epic_app_name;
  std::string ubisoft_space_id;
  std::string ubisoft_game_id;
  int achievements_unlocked = 0;
  int achievements_total = 0;
  bool stats_available = false;
  bool achievements_known = false;
  std::string epic_namespace;
  std::string epic_catalog_id;
  std::string epic_launcher_path;
  std::string launch_uri;
  std::string install_uri;
  std::string store_uri;
  std::string title;
  std::string source;        // Display label: Steam, Faugus, Epic, EA, Ubisoft.
  GameKind kind = GameKind::Steam;
  std::string install_path;
  std::string library_path;
  std::string cover_path;
  std::string hero_path;
  std::string logo_path;
  long long last_played = 0;
  int playtime_minutes = 0;
  long long size_on_disk = 0;
  bool installed = false;
  bool flatpak = false;
  bool launcher = false;          // Faugus entry that is a store client.
  std::string executable;         // Store game executable.
  std::string runner;             // Faugus Proton runner.
  std::string launch_arguments;   // Store client environment.
  GameFlags flags;

  [[nodiscard]] bool is_steam() const { return kind == GameKind::Steam; }
};

struct LibraryFilter {
  std::string search;
  std::string sort = "title";
  std::string source = "all";
  bool favorites_only = false;
  bool show_hidden = false;
  bool installed_only = false;
};

// Merges Steam and Faugus records with local flags and applies the current view.
[[nodiscard]] std::vector<Game> load_games(const SteamScanResult& steam,
                                           const FaugusScanResult& faugus,
                                           const StoreScanResult& stores,
                                           FlagsStore& flags);
[[nodiscard]] std::vector<Game> filter_games(const std::vector<Game>& games,
                                             const LibraryFilter& filter);
[[nodiscard]] std::string normalise_title(const std::string& value);

} // namespace gameslauncher
