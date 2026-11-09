#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gameslauncher {

struct EpicSession {
  std::string access_token;
  std::string refresh_token;
  std::string account_id;
  std::string display_name;
  long long expires_at = 0;
};

struct EpicGame {
  std::string app_name;   // Epic AppName used by manifests and the launcher.
  std::string namespace_id;
  std::string catalog_id;
  std::string title;
  std::string cover_url;
  std::string cover_path;  // Filled by the caller after caching the image.
};

// Appends a non-secret diagnostic line to the Epic log.
void epic_log(const std::string& message);

// Epic Games Store OAuth and owned-library client. Tokens are held in memory
// and persisted by the caller in the desktop Secret Service.
class EpicClient {
public:
  // URL for the embedded login window. After sign-in Epic redirects to a page
  // containing the authorization code.
  [[nodiscard]] static std::string login_url();

  // Exchanges an authorization code for a session.
  [[nodiscard]] static bool exchange_code(const std::string& code, EpicSession* session,
                                          std::string* error);

  // Refreshes an existing session in place.
  [[nodiscard]] static bool refresh(EpicSession* session, std::string* error);

  // Fetches the owned games that are launchable on Windows.
  [[nodiscard]] static bool fetch_owned_games(const EpicSession& session,
                                              std::vector<EpicGame>* games,
                                              std::string* error);

  [[nodiscard]] static bool save_session(const EpicSession& session);
  [[nodiscard]] static std::optional<EpicSession> load_session();
  static void clear_session();
};

} // namespace gameslauncher
