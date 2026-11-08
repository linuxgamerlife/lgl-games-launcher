#pragma once

#include <string>
#include <vector>

namespace gameslauncher {

struct SteamGame {
  std::string app_id;
  std::string name;
  std::string install_dir;
  std::string install_path;
  std::string library_path;
  std::string manifest_path;
  std::string cover_path;
  std::string hero_path;
  std::string logo_path;
  long long last_played = 0;
  int playtime_minutes = 0;
  long long size_on_disk = 0;
  bool installed = false;
};

struct SteamScanResult {
  std::vector<SteamGame> games;
  std::vector<std::string> roots;
  std::vector<std::string> warnings;
  bool flatpak = false;
};

class SteamScanner {
public:
  // Reads only Steam's local manifests. It never writes to Steam's tree.
  [[nodiscard]] static SteamScanResult scan();
  [[nodiscard]] static std::vector<std::string> discover_roots();
  [[nodiscard]] static bool is_tool_title(const std::string& name);

  // Exposed for tests.
  [[nodiscard]] static std::string find_cover(const std::string& root,
                                              const std::string& app_id);
  [[nodiscard]] static std::string find_hero(const std::string& root,
                                             const std::string& app_id);
  [[nodiscard]] static std::string find_logo(const std::string& root,
                                             const std::string& app_id);
};

} // namespace gameslauncher
