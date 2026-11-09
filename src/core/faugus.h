#pragma once

#include <string>
#include <vector>

namespace gameslauncher {

struct FaugusGame {
  std::string gameid;
  std::string title;
  std::string path;
  std::string prefix;
  std::string icon_path;
  std::string cover_path;
  std::string source;
  std::string runner;
  std::string launch_arguments;
  int playtime_minutes = 0;
  long long last_played = 0;
};

struct FaugusScanResult {
  std::vector<FaugusGame> games;
  std::vector<std::string> warnings;
  bool available = false;
  bool flatpak = false;
};

class FaugusScanner {
public:
  // Reads Faugus's local games.json. It never writes to Faugus's tree.
  [[nodiscard]] static FaugusScanResult scan();

  // Native path, or the Flatpak data path when the native one is absent.
  [[nodiscard]] static std::string games_path();

  // Canonical source label: Faugus, Epic, EA or Ubisoft.
  [[nodiscard]] static std::string detect_source(const std::string& gameid,
                                                 const std::string& title,
                                                 const std::string& path);
};

} // namespace gameslauncher
