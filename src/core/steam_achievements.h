#pragma once

#include <string>
#include <vector>

namespace gameslauncher {

struct SteamAchievement {
  std::string api_name;
  std::string name;
  std::string description;
  std::string icon_url;
  std::string icon_path;
  bool unlocked = false;
  long long unlock_time = 0;
  bool hidden = false;
};

struct SteamAchievementsResult {
  std::vector<SteamAchievement> achievements;
  std::string error;
  bool available = false;
};

// Fetches the public Steam Community achievement XML for a profile. The
// profile must be public. Only numeric ids are accepted; no credentials are
// sent.
SteamAchievementsResult fetch_steam_achievements(const std::string& steam_id,
                                                 const std::string& app_id);

} // namespace gameslauncher
