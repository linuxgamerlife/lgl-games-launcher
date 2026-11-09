#pragma once

#include "core/faugus.h"

#include <string>
#include <vector>

namespace gameslauncher {

enum class StoreKind {
  Ubisoft,
  EA,
  Epic,
};

struct StoreGame {
  std::string id;              // Stable per store id.
  std::string app_name;        // Epic AppName, when known.
  std::string namespace_id;    // Epic catalog namespace.
  std::string catalog_id;      // Epic catalog item id.
  std::string launcher_path;   // Store launcher executable from Faugus.
  std::string title;
  std::string install_dir;     // Unix path.
  std::string executable;      // Unix path when known.
  std::string prefix;          // Wine prefix that owns the install.
  std::string launcher_gameid; // Faugus entry used to open the store.
  std::string runner;          // Proton runner from the launcher entry.
  std::string launch_arguments; // Store client environment assignments.
  StoreKind store = StoreKind::Ubisoft;
  std::string source;          // Ubisoft, EA or Epic.
};

struct StoreScanResult {
  std::vector<StoreGame> games;
  std::vector<std::string> warnings;
};

// Finds games installed by the store clients that Faugus manages. It scans the
// client's own registry and manifests inside each Faugus prefix, so games are
// found wherever the store installed them. It never writes to the prefix.
class StoreScanner {
public:
  [[nodiscard]] static StoreScanResult scan(const FaugusScanResult& faugus);

  // Exposed for tests and reuse.
  [[nodiscard]] static std::string windows_to_unix(const std::string& prefix,
                                                   const std::string& windows_path);
};

} // namespace gameslauncher
