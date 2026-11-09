#include "core/igdb.h"

#include <glib.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <string>
#include <utility>

namespace gameslauncher {
namespace {

constexpr gsize kMaxResponseBytes = 8 * 1024 * 1024;
constexpr guint kTimeoutSeconds = 15;

bool numeric(const std::string& value) {
  return !value.empty() && value.size() <= 32 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return std::isdigit(ch) != 0;
         });
}

std::string escape(const std::string& value) {
  gchar* escaped = g_uri_escape_string(value.c_str(), nullptr, false);
  std::string result = escaped != nullptr ? escaped : "";
  g_free(escaped);
  return result;
}

long long now_seconds() { return static_cast<long long>(std::time(nullptr)); }

std::string current_date() {
  const std::time_t value = std::time(nullptr);
  std::tm local{};
  localtime_r(&value, &local);
  char buffer[32]{};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
  return buffer;
}

std::string json_string(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node)) {
    return {};
  }
  return json_node_get_string(node) != nullptr ? json_node_get_string(node) : "";
}

double json_number(JsonObject* object, const char* member, double fallback = 0.0) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return fallback;
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node)) {
    return fallback;
  }
  const GType type = json_node_get_value_type(node);
  if (type == G_TYPE_INT64 || type == G_TYPE_INT) {
    return static_cast<double>(json_node_get_int(node));
  }
  return json_node_get_double(node);
}

} // namespace

IgdbClient::IgdbClient(std::string client_id, std::string client_secret)
    : m_client_id(std::move(client_id)), m_client_secret(std::move(client_secret)) {}

IgdbClient::~IgdbClient() = default;

bool IgdbClient::configured() const {
  return !m_client_id.empty() && !m_client_secret.empty();
}

bool IgdbClient::ensure_token(std::string* error) {
  if (!m_token.empty() && now_seconds() + 60 < m_expires_at) {
    return true;
  }
  if (!configured()) {
    if (error != nullptr) {
      *error = "IGDB is not connected.";
    }
    return false;
  }
  const std::string body = "client_id=" + escape(m_client_id) +
                           "&client_secret=" + escape(m_client_secret) +
                           "&grant_type=client_credentials";
  std::string response;
  if (!post("https://id.twitch.tv/oauth2/token", body,
            "application/x-www-form-urlencoded", {}, {}, &response, error)) {
    return false;
  }
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, response.c_str(), static_cast<gssize>(response.size()),
                                  nullptr)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "IGDB authentication returned malformed data.";
    }
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "IGDB authentication returned unexpected data.";
    }
    return false;
  }
  JsonObject* object = json_node_get_object(root);
  m_token = json_string(object, "access_token");
  const double expires = json_number(object, "expires_in", 0.0);
  g_object_unref(parser);
  if (m_token.empty()) {
    if (error != nullptr) {
      *error = "IGDB authentication did not return a token.";
    }
    return false;
  }
  m_expires_at = now_seconds() + static_cast<long long>(expires);
  return true;
}

bool IgdbClient::post(const std::string& url, const std::string& body,
                      const std::string& content_type, const std::string& client_id,
                      const std::string& token, std::string* response, std::string* error) {
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new("POST", url.c_str());
  if (message == nullptr) {
    g_object_unref(session);
    if (error != nullptr) {
      *error = "Could not create the IGDB request.";
    }
    return false;
  }
  SoupMessageHeaders* request_headers = soup_message_get_request_headers(message);
  soup_message_headers_append(request_headers, "Accept", "application/json");
  if (!client_id.empty()) {
    soup_message_headers_append(request_headers, "Client-ID", client_id.c_str());
  }
  if (!token.empty()) {
    const std::string authorization = "Bearer " + token;
    soup_message_headers_append(request_headers, "Authorization", authorization.c_str());
  }
  GBytes* request_body = g_bytes_new(body.data(), body.size());
  soup_message_set_request_body_from_bytes(message, content_type.c_str(), request_body);
  g_bytes_unref(request_body);

  GError* request_error = nullptr;
  GBytes* bytes = soup_session_send_and_read(session, message, nullptr, &request_error);
  const guint status = soup_message_get_status(message);
  const gsize size = bytes != nullptr ? g_bytes_get_size(bytes) : 0;
  if (request_error != nullptr) {
    if (error != nullptr) {
      *error = request_error->message;
    }
    g_error_free(request_error);
  } else if (status < 200 || status >= 300) {
    if (error != nullptr) {
      *error = "IGDB returned HTTP " + std::to_string(status) + ".";
    }
  } else if (bytes == nullptr || size == 0 || size > kMaxResponseBytes) {
    if (error != nullptr) {
      *error = "IGDB returned an empty or oversized response.";
    }
  } else {
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes, nullptr));
    response->assign(data, size);
  }
  if (bytes != nullptr) {
    g_bytes_unref(bytes);
  }
  g_object_unref(message);
  g_object_unref(session);
  return response != nullptr && !response->empty();
}

bool IgdbClient::fetch(const std::string& steam_app_id, const std::string& title,
                       IgdbInfo* info, std::string* error) {
  if (info == nullptr) {
    if (error != nullptr) {
      *error = "IGDB request is invalid.";
    }
    return false;
  }
  if (!steam_app_id.empty() && !numeric(steam_app_id)) {
    if (error != nullptr) {
      *error = "Invalid Steam app id.";
    }
    return false;
  }
  if (!ensure_token(error)) {
    return false;
  }
  std::string response;
  double game_id = 0.0;
  std::string last_error;
  const std::string external_query =
      "fields game; where uid = \"" + steam_app_id + "\" & category = 1; limit 1;";
  if (!steam_app_id.empty() &&
      post("https://api.igdb.com/v4/external_games", external_query, "text/plain",
           m_client_id, m_token, &response, &last_error)) {
    JsonParser* parser = json_parser_new();
    if (json_parser_load_from_data(parser, response.c_str(),
                                   static_cast<gssize>(response.size()), nullptr)) {
      JsonNode* root = json_parser_get_root(parser);
      JsonArray* external = (root != nullptr && JSON_NODE_HOLDS_ARRAY(root))
                                ? json_node_get_array(root)
                                : nullptr;
      if (external != nullptr && json_array_get_length(external) > 0) {
        JsonObject* first = json_array_get_object_element(external, 0);
        game_id = static_cast<double>(json_object_get_int_member_with_default(first, "game", 0));
      }
    }
    g_object_unref(parser);
  }

  // Some Steam apps (especially demos and newer releases) have no IGDB
  // external-game mapping. Fall back to IGDB search, accepting only an exact
  // case-insensitive title match so a wrong game is not shown.
  if (game_id <= 0.0 && !title.empty()) {
    std::string escaped;
    escaped.reserve(title.size());
    for (char ch : title) {
      if (ch == '\\' || ch == '"') {
        escaped.push_back('\\');
      }
      escaped.push_back(ch);
    }
    const std::string search_query =
        "search \"" + escaped + "\"; fields name,aggregated_rating,"
        "aggregated_rating_count,total_rating,total_rating_count,url; limit 10;";
    std::string search_response;
    if (post("https://api.igdb.com/v4/games", search_query, "text/plain", m_client_id,
             m_token, &search_response, &last_error)) {
      JsonParser* parser = json_parser_new();
      if (json_parser_load_from_data(parser, search_response.c_str(),
                                     static_cast<gssize>(search_response.size()), nullptr)) {
        JsonNode* search_root = json_parser_get_root(parser);
        JsonArray* games = (search_root != nullptr && JSON_NODE_HOLDS_ARRAY(search_root))
                               ? json_node_get_array(search_root)
                               : nullptr;
        const std::string wanted = [&title] {
          std::string normalized;
          for (unsigned char ch : title) {
            if (std::isalnum(ch) != 0) {
              normalized.push_back(static_cast<char>(std::tolower(ch)));
            }
          }
          return normalized;
        }();
        for (guint index = 0; games != nullptr && index < json_array_get_length(games); ++index) {
          JsonObject* candidate = json_array_get_object_element(games, index);
          const std::string name = json_string(candidate, "name");
          std::string normalized;
          for (unsigned char ch : name) {
            if (std::isalnum(ch) != 0) {
              normalized.push_back(static_cast<char>(std::tolower(ch)));
            }
          }
          if (normalized == wanted) {
            game_id = static_cast<double>(json_object_get_int_member_with_default(candidate, "id", 0));
            break;
          }
        }
      }
      g_object_unref(parser);
    }
  }

  if (game_id <= 0.0) {
    if (error != nullptr) {
      *error = last_error.empty() ? "IGDB has no match for this game." : last_error;
    }
    return false;
  }

  JsonParser* parser = nullptr;
  const std::string game_query =
      "fields aggregated_rating,aggregated_rating_count,total_rating,total_rating_count,url; "
      "where id = " + std::to_string(static_cast<long long>(game_id)) + ";";
  if (!post("https://api.igdb.com/v4/games", game_query, "text/plain", m_client_id, m_token,
            &response, error)) {
    return false;
  }
  parser = json_parser_new();
  if (!json_parser_load_from_data(parser, response.c_str(), static_cast<gssize>(response.size()),
                                  nullptr)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "IGDB returned malformed game data.";
    }
    return false;
  }
  JsonNode* game_root = json_parser_get_root(parser);
  JsonArray* games = (game_root != nullptr && JSON_NODE_HOLDS_ARRAY(game_root))
                         ? json_node_get_array(game_root)
                         : nullptr;
  if (games == nullptr || json_array_get_length(games) == 0) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "IGDB returned no game details.";
    }
    return false;
  }
  JsonObject* game = json_array_get_object_element(games, 0);
  info->critic_score = static_cast<int>(json_number(game, "aggregated_rating", 0.0));
  info->critic_count = static_cast<int>(json_number(game, "aggregated_rating_count", 0.0));
  if (info->critic_score == 0) {
    info->critic_score = static_cast<int>(json_number(game, "total_rating", 0.0));
  }
  info->url = json_string(game, "url");
  g_object_unref(parser);

  const std::string time_query =
      "fields hastily,normally,completely; where game_id = " +
      std::to_string(static_cast<long long>(game_id)) + "; limit 1;";
  if (post("https://api.igdb.com/v4/game_time_to_beats", time_query, "text/plain",
           m_client_id, m_token, &response, error)) {
    parser = json_parser_new();
    if (json_parser_load_from_data(parser, response.c_str(), static_cast<gssize>(response.size()),
                                   nullptr)) {
      JsonNode* time_root = json_parser_get_root(parser);
      JsonArray* times = (time_root != nullptr && JSON_NODE_HOLDS_ARRAY(time_root))
                             ? json_node_get_array(time_root)
                             : nullptr;
      if (times != nullptr && json_array_get_length(times) > 0) {
        JsonObject* time = json_array_get_object_element(times, 0);
        info->rushed_hours = static_cast<int>(json_number(time, "hastily", 0.0) / 3600.0);
        info->normal_hours = static_cast<int>(json_number(time, "normally", 0.0) / 3600.0);
        info->complete_hours = static_cast<int>(json_number(time, "completely", 0.0) / 3600.0);
      }
    }
    g_object_unref(parser);
  }
  info->available = true;
  info->updated = current_date();
  return true;
}

} // namespace gameslauncher
