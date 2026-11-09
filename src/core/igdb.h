#pragma once

#include <string>

namespace gameslauncher {

struct IgdbInfo {
  bool available = false;
  int critic_score = 0;
  int critic_count = 0;
  int rushed_hours = 0;
  int normal_hours = 0;
  int complete_hours = 0;
  std::string url;
  std::string updated;
  std::string error;
};

// Minimal IGDB client: Twitch client-credentials OAuth, then IGDB v4.
// The secret is held in memory only by the caller and is never logged.
class IgdbClient {
public:
  IgdbClient(std::string client_id, std::string client_secret);
  ~IgdbClient();

  IgdbClient(const IgdbClient&) = delete;
  IgdbClient& operator=(const IgdbClient&) = delete;

  [[nodiscard]] bool configured() const;
  bool fetch(const std::string& steam_app_id, const std::string& title,
             IgdbInfo* info, std::string* error);

private:
  bool ensure_token(std::string* error);
  bool post(const std::string& url, const std::string& body,
            const std::string& content_type, const std::string& client_id,
            const std::string& token, std::string* response, std::string* error);

  std::string m_client_id;
  std::string m_client_secret;
  std::string m_token;
  long long m_expires_at = 0;
};

} // namespace gameslauncher
