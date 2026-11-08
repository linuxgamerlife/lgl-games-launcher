#pragma once

#include <string>

struct sqlite3;

namespace gameslauncher {

struct GameFlags {
  bool favorite = false;
  bool hidden = false;
  std::string tags;
};

// Per-app user choices. The Steam manifests are the source of truth for the
// game list; only personal flags live here.
class FlagsStore {
public:
  FlagsStore() = default;
  ~FlagsStore();
  FlagsStore(const FlagsStore&) = delete;
  FlagsStore& operator=(const FlagsStore&) = delete;
  FlagsStore(FlagsStore&&) = delete;
  FlagsStore& operator=(FlagsStore&&) = delete;

  bool open(const std::string& path, std::string* error = nullptr);
  [[nodiscard]] bool is_open() const { return m_db != nullptr; }
  [[nodiscard]] GameFlags get(const std::string& app_id) const;
  bool set(const std::string& app_id, const GameFlags& flags);

private:
  bool ensure_schema(std::string* error);

  sqlite3* m_db = nullptr;
};

} // namespace gameslauncher
