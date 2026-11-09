#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gameslauncher {

// Appends a non-secret diagnostic line to the Ubisoft log.
void ubisoft_log(const std::string& message);

struct UbisoftSession {
  std::string ticket;
  std::string session_id;
  std::string remember_me_ticket;
  std::string user_id;
  std::string username;
  long long refresh_time = 0;
};

struct UbisoftGame {
  std::string game_id;
  std::string space_id;
  std::string name;
  std::string cover_url;
  std::string banner_url;
  std::string background_url;
  std::string cover_path;
  std::string hero_path;
  int playtime_minutes = 0;
  int achievements_unlocked = 0;
  int achievements_total = 0;
  bool stats_available = false;
  bool achievements_known = false;
};

struct UbisoftStats {
  int playtime_minutes = 0;
  int achievements_unlocked = 0;
  int achievements_total = 0;
  bool achievements_known = false;
};

// Ubisoft Connect OAuth/session and owned-library client. The login itself runs
// on Ubisoft's own page; only the resulting session is stored here.
class UbisoftClient {
public:
  [[nodiscard]] static std::string login_url();

  [[nodiscard]] static bool authorise_with_storage(const std::string& login_json,
                                                   const std::string& remember_json,
                                                   const std::string& profile_json,
                                                   UbisoftSession* session,
                                                   std::string* error);
  [[nodiscard]] static bool refresh(UbisoftSession* session, std::string* error);
  [[nodiscard]] static bool fetch_owned_games(const UbisoftSession& session,
                                              std::vector<UbisoftGame>* games,
                                              std::string* error);

  // Maps a Ubisoft space id to the numeric launch id used by uplay://.
  // Reads Ubisoft Connect's local configuration cache.
  [[nodiscard]] static std::string launch_id_for(const std::string& prefix,
                                                 const std::string& space_id);

  // Reads playtime and achievement counts from Ubisoft Connect's local cache.
  [[nodiscard]] static std::map<std::string, UbisoftStats> load_local_stats(
      const std::string& prefix);
  [[nodiscard]] static bool fetch_remote_stats(const UbisoftSession& session,
                                               const std::string& space_id,
                                               UbisoftStats* stats,
                                               std::string* error);
  [[nodiscard]] static int achievement_total(const std::string& prefix,
                                             const std::string& space_id);

  static void save_games_cache(const std::vector<UbisoftGame>& games);
  [[nodiscard]] static std::vector<UbisoftGame> load_games_cache();
  static void clear_games_cache();

  [[nodiscard]] static bool save_session(const UbisoftSession& session);
  [[nodiscard]] static std::optional<UbisoftSession> load_session();
  static void clear_session();
};

[[nodiscard]] std::string ubisoft_launch_uri(const std::string& space_id);
[[nodiscard]] std::string ubisoft_install_uri(const std::string& space_id);

} // namespace gameslauncher
