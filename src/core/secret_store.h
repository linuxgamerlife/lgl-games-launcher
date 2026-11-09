#pragma once

#include <string>

namespace gameslauncher {

// Secret Service-backed storage. Secrets are never written to settings.ini,
// the library database, logs or argv.
class SecretStore {
public:
  [[nodiscard]] static std::string lookup(const std::string& purpose);
  [[nodiscard]] static bool store(const std::string& purpose, const std::string& value,
                                  std::string* error = nullptr);
  static bool remove(const std::string& purpose, std::string* error = nullptr);
};

} // namespace gameslauncher
