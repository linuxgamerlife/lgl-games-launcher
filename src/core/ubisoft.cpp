#include "core/ubisoft.h"

#include "core/paths.h"
#include "core/secret_store.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <utility>

namespace gameslauncher {
namespace {

constexpr const char* kAppId = "82b650c0-6cb3-40c0-9f41-25a53b62b206";
constexpr const char* kGenomeId = "42d07c95-9914-4450-8b38-267c4e462b21";
constexpr const char* kApiHost = "public-ubiservices.ubi.com";
constexpr const char* kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/72.0.3626.121 Safari/537.36";
constexpr guint kTimeoutSeconds = 20;
constexpr gsize kMaxResponseBytes = 8 * 1024 * 1024;
constexpr const char* kSecretKey = "ubisoft-session";

bool http(const std::string& method, const std::string& url,
          const std::vector<std::pair<std::string, std::string>>& headers,
          const std::string& body, guint* status, std::string* response,
          std::string* error) {
  if (status != nullptr) {
    *status = 0;
  }
  if (response != nullptr) {
    response->clear();
  }
  if (error != nullptr) {
    error->clear();
  }
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new(method.c_str(), url.c_str());
  if (message == nullptr) {
    g_object_unref(session);
    if (error != nullptr) {
      *error = "Could not create the Ubisoft request.";
    }
    return false;
  }
  SoupMessageHeaders* request_headers = soup_message_get_request_headers(message);
  soup_message_headers_append(request_headers, "User-Agent", kUserAgent);
  soup_message_headers_append(request_headers, "Ubi-AppId", kAppId);
  soup_message_headers_append(request_headers, "Origin", "https://connect.ubisoft.com");
  soup_message_headers_append(request_headers, "Referer", "https://connect.ubisoft.com/");
  for (const auto& [name, value] : headers) {
    soup_message_headers_append(request_headers, name.c_str(), value.c_str());
  }
  if (!body.empty()) {
    GBytes* bytes = g_bytes_new(body.data(), body.size());
    soup_message_set_request_body_from_bytes(message, "application/json", bytes);
    g_bytes_unref(bytes);
  }
  GError* request_error = nullptr;
  GBytes* bytes = soup_session_send_and_read(session, message, nullptr, &request_error);
  const guint response_status = soup_message_get_status(message);
  if (status != nullptr) {
    *status = response_status;
  }
  const gsize size = bytes != nullptr ? g_bytes_get_size(bytes) : 0;
  if (request_error != nullptr) {
    if (error != nullptr) {
      *error = request_error->message;
    }
    g_error_free(request_error);
  } else if (size > kMaxResponseBytes) {
    if (error != nullptr) {
      *error = "The Ubisoft response was too large.";
    }
  } else if (bytes != nullptr) {
    gsize length = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &length));
    if (response != nullptr && data != nullptr) {
      response->assign(data, length);
    }
  }
  if (bytes != nullptr) {
    g_bytes_unref(bytes);
  }
  g_object_unref(message);
  g_object_unref(session);
  return error == nullptr || error->empty();
}

std::string json_string(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) ||
      json_node_get_value_type(node) != G_TYPE_STRING) {
    return {};
  }
  const char* value = json_node_get_string(node);
  return value != nullptr ? value : "";
}

std::string json_value_text(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node)) {
    return {};
  }
  const GType type = json_node_get_value_type(node);
  if (type == G_TYPE_STRING) {
    const char* value = json_node_get_string(node);
    return value != nullptr ? value : "";
  }
  if (type == G_TYPE_INT64 || type == G_TYPE_INT) {
    return std::to_string(json_node_get_int(node));
  }
  if (type == G_TYPE_DOUBLE) {
    return std::to_string(static_cast<long long>(json_node_get_double(node)));
  }
  return {};
}

long long iso_seconds(const std::string& value) {
  if (value.empty()) {
    return 0;
  }
  GDateTime* parsed = g_date_time_new_from_iso8601(value.c_str(), nullptr);
  if (parsed == nullptr) {
    return 0;
  }
  const long long seconds = static_cast<long long>(g_date_time_to_unix(parsed));
  g_date_time_unref(parsed);
  return seconds;
}

std::string session_headers_ticket(const UbisoftSession& session, const char* prefix) {
  return std::string(prefix) + " t=" + session.ticket;
}

void read_session_object(JsonObject* object, UbisoftSession* session) {
  if (object == nullptr) {
    return;
  }
  const std::string ticket = json_string(object, "ticket");
  if (!ticket.empty()) {
    session->ticket = ticket;
  }
  const std::string session_id = json_string(object, "sessionId");
  if (!session_id.empty()) {
    session->session_id = session_id;
  }
  const std::string remember = json_string(object, "rememberMeTicket");
  if (!remember.empty()) {
    session->remember_me_ticket = remember;
  }
  const std::string user = json_string(object, "userId");
  if (!user.empty()) {
    session->user_id = user;
  }
  const std::string name = json_string(object, "nameOnPlatform");
  if (!name.empty()) {
    session->username = name;
  }
  const std::string username = json_string(object, "username");
  if (!username.empty()) {
    session->username = username;
  }
  const std::string expiration = json_string(object, "expiration");
  const std::string server_time = json_string(object, "serverTime");
  if (!expiration.empty() && !server_time.empty()) {
    const long long expires = iso_seconds(expiration);
    const long long server = iso_seconds(server_time);
    if (expires > server && server > 0) {
      session->refresh_time = static_cast<long long>(g_get_real_time() / 1000000) +
                              (expires - server);
    }
  }
}

bool parse_session(const std::string& body, UbisoftSession* session, std::string* error) {
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body.c_str(), static_cast<gssize>(body.size()),
                                  nullptr)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "Ubisoft returned an invalid response.";
    }
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  const std::string error_code = json_string(object, "errorCode");
  if (!error_code.empty()) {
    if (error != nullptr) {
      *error = json_string(object, "message").empty() ? error_code
                                                      : json_string(object, "message");
    }
    g_object_unref(parser);
    return false;
  }
  read_session_object(object, session);
  g_object_unref(parser);
  if (session->ticket.empty()) {
    if (error != nullptr) {
      *error = "Ubisoft did not return a session ticket.";
    }
    return false;
  }
  return true;
}

bool post_session(const UbisoftSession& session, UbisoftSession* result, std::string* error) {
  guint status = 0;
  std::string response;
  const std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", session_headers_ticket(session, "Ubi_v1")},
      {"Ubi-SessionId", session.session_id},
      {"Ubi-RequestedPlatformType", "uplay"},
      {"Ubi-LocaleCode", "en-US"},
  };
  if (!http("POST", std::string("https://") + kApiHost + "/v2/profiles/sessions", headers,
            "{}", &status, &response, error)) {
    return false;
  }
  if (status != 200 && status != 201) {
    if (error != nullptr) {
      *error = "Ubisoft session request failed (HTTP " + std::to_string(status) + ").";
    }
    return false;
  }
  return parse_session(response, result, error);
}

bool refresh_ticket(const UbisoftSession& session, UbisoftSession* result, std::string* error) {
  guint status = 0;
  std::string response;
  const std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", session_headers_ticket(session, "Ubi_v1")},
      {"Ubi-SessionId", session.session_id},
  };
  if (!http("PUT", std::string("https://") + kApiHost + "/v3/profiles/sessions", headers, "{}",
            &status, &response, nullptr) ||
      status != 200) {
    return false;
  }
  return parse_session(response, result, error);
}

bool refresh_remember_me(const UbisoftSession& session, UbisoftSession* result,
                         std::string* error) {
  if (session.remember_me_ticket.empty()) {
    if (error != nullptr) {
      *error = "No Ubisoft remember-me token is available.";
    }
    return false;
  }
  guint status = 0;
  std::string response;
  const std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", "rm_v1 t=" + session.remember_me_ticket},
  };
  if (!http("POST", std::string("https://") + kApiHost + "/v3/profiles/sessions", headers,
            "{\"rememberMe\":true}", &status, &response, error)) {
    return false;
  }
  if (status != 200 && status != 201) {
    if (error != nullptr) {
      *error = "Ubisoft remember-me refresh failed (HTTP " + std::to_string(status) + ").";
    }
    return false;
  }
  return parse_session(response, result, error);
}

std::string graphql_body() {
  const char* query =
      "query AllGames { viewer { id ...ownedGamesList } } "
      "fragment gameProps on Game { id spaceId name coverUrl backgroundUrl bannerUrl } "
      "fragment ownedGameProps on Game { ...gameProps viewer { meta { id "
      "ownedPlatformGroups { id name type } } } } "
      "fragment ownedGamesList on User { ownedGames: games(filterBy: {isOwned: true}) { "
      "totalCount nodes { ...ownedGameProps } } }";
  JsonObject* object = json_object_new();
  json_object_set_string_member(object, "operationName", "AllGames");
  JsonObject* variables = json_object_new();
  json_object_set_boolean_member(variables, "owned", TRUE);
  json_object_set_object_member(object, "variables", variables);
  json_object_set_string_member(object, "query", query);
  JsonNode* root = json_node_alloc();
  json_node_init_object(root, object);
  JsonGenerator* generator = json_generator_new();
  json_generator_set_root(generator, root);
  gchar* data = json_generator_to_data(generator, nullptr);
  std::string result = data != nullptr ? data : "{}";
  g_free(data);
  g_object_unref(generator);
  json_node_free(root);
  return result;
}

bool group_has_pc(JsonNode* node) {
  if (node == nullptr) {
    return false;
  }
  if (JSON_NODE_HOLDS_OBJECT(node)) {
    JsonObject* object = json_node_get_object(node);
    return json_string(object, "type") == "PC";
  }
  if (JSON_NODE_HOLDS_ARRAY(node)) {
    JsonArray* array = json_node_get_array(node);
    for (guint index = 0; array != nullptr && index < json_array_get_length(array); ++index) {
      if (group_has_pc(json_array_get_element(array, index))) {
        return true;
      }
    }
  }
  return false;
}

bool platform_is_pc(JsonObject* game) {
  if (game == nullptr) {
    return false;
  }
  JsonObject* viewer = json_object_has_member(game, "viewer")
                           ? json_object_get_object_member(game, "viewer")
                           : nullptr;
  JsonObject* meta = (viewer != nullptr && json_object_has_member(viewer, "meta"))
                         ? json_object_get_object_member(viewer, "meta")
                         : nullptr;
  if (meta == nullptr || !json_object_has_member(meta, "ownedPlatformGroups")) {
    // Older entries do not carry platform groups; include them like Lutris.
    return true;
  }
  JsonArray* groups = json_object_get_array_member(meta, "ownedPlatformGroups");
  for (guint index = 0; groups != nullptr && index < json_array_get_length(groups); ++index) {
    if (group_has_pc(json_array_get_element(groups, index))) {
      return true;
    }
  }
  return false;
}

} // namespace

void ubisoft_log(const std::string& message) {
  const std::string path = Glib::build_filename(cache_dir(), "ubisoft.log");
  g_mkdir_with_parents(cache_dir().c_str(), 0700);
  FILE* file = g_fopen(path.c_str(), "a");
  if (file != nullptr) {
    g_chmod(path.c_str(), 0600);
    std::fprintf(file, "%s\n", message.c_str());
    std::fclose(file);
  }
}

std::string UbisoftClient::login_url() {
  return "https://connect.ubisoft.com/login?appId=" + std::string(kAppId) +
         "&genomeId=" + std::string(kGenomeId) +
         "&lang=en-US&nextUrl=https:%2F%2Fconnect.ubisoft.com%2Fready";
}

void collect_node(JsonNode* node, UbisoftSession* session, int depth);

void collect_storage_value(const std::string& raw, UbisoftSession* session, int depth) {
  if (raw.empty() || depth > 3) {
    return;
  }
  std::string value = raw;
  if (value.find('%') != std::string::npos) {
    gchar* decoded = g_uri_unescape_string(value.c_str(), nullptr);
    if (decoded != nullptr) {
      value = decoded;
      g_free(decoded);
    }
  }
  if (depth == 0) {
    ubisoft_log("login: storage len=" + std::to_string(value.size()) +
                " encoded=" + (raw.find('%') != std::string::npos ? "yes" : "no"));
  }
  JsonParser* parser = json_parser_new();
  if (json_parser_load_from_data(parser, value.c_str(), static_cast<gssize>(value.size()),
                                 nullptr)) {
    collect_node(json_parser_get_root(parser), session, depth);
  } else if (depth == 0) {
    ubisoft_log("login: storage is not JSON");
  }
  g_object_unref(parser);
}

void collect_node(JsonNode* node, UbisoftSession* session, int depth) {
  if (node == nullptr || depth > 3) {
    return;
  }
  if (JSON_NODE_HOLDS_OBJECT(node)) {
    JsonObject* object = json_node_get_object(node);
    if (depth == 0) {
      GList* members = json_object_get_members(object);
      std::string keys;
      for (GList* item = members; item != nullptr; item = item->next) {
        if (!keys.empty()) {
          keys += ",";
        }
        keys += static_cast<const char*>(item->data);
      }
      g_list_free(members);
      ubisoft_log("login: keys=" + keys);
    }
    read_session_object(object, session);
    GList* members = json_object_get_members(object);
    for (GList* item = members; item != nullptr; item = item->next) {
      const char* name = static_cast<const char*>(item->data);
      JsonNode* child = json_object_get_member(object, name);
      if (child == nullptr) {
        continue;
      }
      if (JSON_NODE_HOLDS_OBJECT(child) || JSON_NODE_HOLDS_ARRAY(child)) {
        collect_node(child, session, depth + 1);
      } else if (JSON_NODE_HOLDS_VALUE(child) &&
                 json_node_get_value_type(child) == G_TYPE_STRING) {
        const char* text = json_node_get_string(child);
        if (text != nullptr) {
          const std::string candidate = text;
          if (candidate.find('{') != std::string::npos ||
              candidate.find("%7B") != std::string::npos ||
              candidate.find("ticket") != std::string::npos) {
            collect_storage_value(candidate, session, depth + 1);
          }
        }
      }
    }
    g_list_free(members);
  } else if (JSON_NODE_HOLDS_ARRAY(node)) {
    JsonArray* array = json_node_get_array(node);
    for (guint index = 0; array != nullptr && index < json_array_get_length(array); ++index) {
      collect_node(json_array_get_element(array, index), session, depth + 1);
    }
  } else if (JSON_NODE_HOLDS_VALUE(node) &&
             json_node_get_value_type(node) == G_TYPE_STRING) {
    const char* text = json_node_get_string(node);
    if (text != nullptr) {
      collect_storage_value(text, session, depth + 1);
    }
  }
}

bool UbisoftClient::authorise_with_storage(const std::string& login_json,
                                           const std::string& remember_json,
                                           const std::string& profile_json,
                                           UbisoftSession* session, std::string* error) {
  if (session == nullptr) {
    return false;
  }
  ubisoft_log("login: storage received");
  UbisoftSession parsed;
  for (const std::string* value : {&login_json, &remember_json, &profile_json}) {
    if (!value->empty()) {
      collect_storage_value(*value, &parsed, 0);
    }
  }
  if (parsed.ticket.empty()) {
    if (error != nullptr) {
      *error = "Ubisoft did not return a login ticket.";
    }
    ubisoft_log("login: missing ticket");
    return false;
  }
  UbisoftSession activated;
  if (!post_session(parsed, &activated, error)) {
    ubisoft_log("login: post_session failed " + (error != nullptr ? *error : std::string()));
    return false;
  }
  for (const std::string* field : {&parsed.user_id, &parsed.username, &parsed.remember_me_ticket}) {
    if (!field->empty()) {
      if (field == &parsed.user_id) {
        activated.user_id = *field;
      } else if (field == &parsed.username) {
        activated.username = *field;
      } else {
        activated.remember_me_ticket = *field;
      }
    }
  }
  *session = std::move(activated);
  ubisoft_log("login: ok");
  return true;
}

bool UbisoftClient::refresh(UbisoftSession* session, std::string* error) {
  if (session == nullptr) {
    return false;
  }
  UbisoftSession refreshed;
  if (refresh_ticket(*session, &refreshed, error)) {
    // Preserve fields the refresh response may omit.
    if (refreshed.user_id.empty()) {
      refreshed.user_id = session->user_id;
    }
    if (refreshed.username.empty()) {
      refreshed.username = session->username;
    }
    if (refreshed.remember_me_ticket.empty()) {
      refreshed.remember_me_ticket = session->remember_me_ticket;
    }
    *session = std::move(refreshed);
    ubisoft_log("refresh: ok");
    return true;
  }
  UbisoftSession remembered;
  if (refresh_remember_me(*session, &remembered, error)) {
    if (remembered.user_id.empty()) {
      remembered.user_id = session->user_id;
    }
    if (remembered.username.empty()) {
      remembered.username = session->username;
    }
    *session = std::move(remembered);
    ubisoft_log("refresh: remember-me ok");
    return true;
  }
  ubisoft_log("refresh: failed " + (error != nullptr ? *error : std::string()));
  return false;
}

bool UbisoftClient::fetch_owned_games(const UbisoftSession& session,
                                      std::vector<UbisoftGame>* games, std::string* error) {
  if (games == nullptr || session.ticket.empty()) {
    if (error != nullptr) {
      *error = "Ubisoft is not connected.";
    }
    return false;
  }
  games->clear();
  guint status = 0;
  std::string response;
  const std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", session_headers_ticket(session, "Ubi_v1")},
      {"Ubi-SessionId", session.session_id},
      {"Ubi-RequestedPlatformType", "uplay"},
      {"Ubi-LocaleCode", "en-US"},
  };
  if (!http("POST", std::string("https://") + kApiHost + "/v1/profiles/me/uplay/graphql",
            headers, graphql_body(), &status, &response, error)) {
    return false;
  }
  if (status != 200) {
    if (error != nullptr) {
      *error = "Ubisoft library request failed (HTTP " + std::to_string(status) + ").";
    }
    ubisoft_log("library: HTTP " + std::to_string(status));
    return false;
  }
  ubisoft_log("library: response bytes=" + std::to_string(response.size()));
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, response.c_str(),
                                  static_cast<gssize>(response.size()), nullptr)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "Ubisoft returned an invalid library response.";
    }
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  if (object != nullptr && json_object_has_member(object, "errors")) {
    JsonArray* errors = json_object_get_array_member(object, "errors");
    for (guint index = 0; errors != nullptr && index < json_array_get_length(errors);
         ++index) {
      JsonObject* item = json_array_get_object_element(errors, index);
      if (item != nullptr) {
        ubisoft_log("library: graphql error " + json_string(item, "message"));
      }
    }
  }
  JsonObject* data =
      (object != nullptr && json_object_has_member(object, "data"))
          ? json_object_get_object_member(object, "data")
          : nullptr;
  JsonObject* viewer =
      (data != nullptr && json_object_has_member(data, "viewer"))
          ? json_object_get_object_member(data, "viewer")
          : nullptr;
  JsonObject* owned =
      (viewer != nullptr && json_object_has_member(viewer, "ownedGames"))
          ? json_object_get_object_member(viewer, "ownedGames")
          : nullptr;
  JsonArray* nodes =
      (owned != nullptr && json_object_has_member(owned, "nodes"))
          ? json_object_get_array_member(owned, "nodes")
          : nullptr;
  ubisoft_log("library: nodes=" +
              std::to_string(nodes != nullptr ? json_array_get_length(nodes) : 0));
  for (guint index = 0; nodes != nullptr && index < json_array_get_length(nodes); ++index) {
    JsonObject* game = json_array_get_object_element(nodes, index);
    if (game == nullptr) {
      continue;
    }
    if (index < 5) {
      ubisoft_log("library: node " + json_string(game, "name") + " id=" +
                  json_string(game, "id") + " space=" + json_string(game, "spaceId") +
                  " pc=" + (platform_is_pc(game) ? "yes" : "no"));
    }
    if (!platform_is_pc(game)) {
      continue;
    }
    UbisoftGame entry;
    entry.game_id = json_string(game, "id");
    entry.space_id = json_string(game, "spaceId");
    if (entry.space_id.empty()) {
      entry.space_id = json_string(game, "id");
    }
    entry.name = json_string(game, "name");
    entry.cover_url = json_string(game, "coverUrl");
    entry.banner_url = json_string(game, "bannerUrl");
    entry.background_url = json_string(game, "backgroundUrl");
    if (entry.space_id.empty() || entry.name.empty()) {
      continue;
    }
    games->push_back(std::move(entry));
  }
  g_object_unref(parser);
  ubisoft_log("library: " + std::to_string(games->size()) + " games");
  return true;
}

namespace {

std::string extract_string_value(const std::string& data, std::size_t key_pos) {
  const std::size_t colon = data.find(':', key_pos);
  if (colon == std::string::npos) {
    return {};
  }
  const std::size_t first = data.find('"', colon);
  if (first == std::string::npos) {
    return {};
  }
  const std::size_t last = data.find('"', first + 1);
  if (last == std::string::npos) {
    return {};
  }
  return data.substr(first + 1, last - first - 1);
}

std::string extract_number_after(const std::string& data, const std::string& key,
                                 std::size_t from, std::size_t to) {
  if (from >= to || to > data.size()) {
    return {};
  }
  const std::size_t pos = data.find(key, from);
  if (pos == std::string::npos || pos >= to) {
    return {};
  }
  const std::size_t colon = data.find(':', pos);
  if (colon == std::string::npos || colon >= to) {
    return {};
  }
  std::size_t index = colon + 1;
  while (index < to && (data[index] == ' ' || data[index] == '	' || data[index] == '\n' ||
                        data[index] == '\r')) {
    ++index;
  }
  const std::size_t start = index;
  while (index < to && data[index] >= '0' && data[index] <= '9') {
    ++index;
  }
  return index > start ? data.substr(start, index - start) : std::string();
}

std::map<std::string, std::string> build_launch_map(const std::string& prefix) {
  std::map<std::string, std::string> preferred;
  std::map<std::string, std::string> fallback;
  const std::filesystem::path roots[] = {
      std::filesystem::path(prefix) /
          "drive_c/users/steamuser/AppData/Local/Ubisoft Game Launcher/cache",
      std::filesystem::path(prefix) / "drive_c/ProgramData/Ubisoft/Ubisoft Game Launcher/cache",
  };
  for (const auto& root : roots) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
      continue;
    }
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, error),
         end;
         !error && it != end; it.increment(error)) {
      std::error_code entry_error;
      if (!it->is_regular_file(entry_error) || it->file_size(entry_error) > 20 * 1024 * 1024) {
        continue;
      }
      std::ifstream file(it->path(), std::ios::binary);
      if (!file) {
        continue;
      }
      std::string data((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
      std::size_t pos = 0;
      while ((pos = data.find("\"space_id\"", pos)) != std::string::npos) {
        const std::string space = extract_string_value(data, pos);
        pos += 10;
        if (space.size() != 36) {
          continue;
        }
        const std::size_t from = pos > 4096 ? pos - 4096 : 0;
        const std::size_t to = std::min(data.size(), pos + 4096);
        std::string activation =
            extract_number_after(data, "\"after_activation_product_id\"", pos, to);
        if (activation.empty()) {
          activation =
              extract_number_after(data, "\"after_activation_product_id\"", from, pos);
        }
        if (!activation.empty()) {
          preferred[space] = activation;
          continue;
        }
        std::string product = extract_number_after(data, "\"product_id\"", pos, to);
        if (product.empty()) {
          product = extract_number_after(data, "\"product_id\"", from, pos);
        }
        if (!product.empty() && fallback.find(space) == fallback.end()) {
          fallback[space] = product;
        }
      }
    }
  }
  std::map<std::string, std::string> map = std::move(fallback);
  for (const auto& [space, launch] : preferred) {
    map[space] = launch;
  }
  return map;
}

} // namespace

std::string UbisoftClient::launch_id_for(const std::string& prefix,
                                        const std::string& space_id) {
  static std::mutex mutex;
  static std::map<std::string, std::map<std::string, std::string>> cache;
  std::lock_guard lock(mutex);
  auto& map = cache[prefix];
  if (map.empty()) {
    map = build_launch_map(prefix);
    ubisoft_log("launch map: " + std::to_string(map.size()) + " entries");
  }
  const auto found = map.find(space_id);
  return found == map.end() ? std::string() : found->second;
}

namespace {

int stat_number(const std::string& value) {
  if (value.empty()) {
    return 0;
  }
  return static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
}

} // namespace

std::map<std::string, UbisoftStats> UbisoftClient::load_local_stats(const std::string& prefix) {
  std::map<std::string, UbisoftStats> result;
  const std::filesystem::path directory =
      std::filesystem::path(prefix) /
      "drive_c/ProgramData/Ubisoft/Ubisoft Game Launcher/cache/game_stats";
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error)) {
    return result;
  }
  for (std::filesystem::directory_iterator it(
           directory, std::filesystem::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    std::error_code entry_error;
    if (!it->is_regular_file(entry_error)) {
      continue;
    }
    std::ifstream file(it->path(), std::ios::binary);
    if (!file) {
      continue;
    }
    std::string data((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    std::size_t pos = 0;
    while ((pos = data.find("{\"Statscards\"", pos)) != std::string::npos) {
      std::size_t uuid_pos = data.rfind("\n$", pos);
      if (uuid_pos == std::string::npos || uuid_pos + 38 > pos) {
        ++pos;
        continue;
      }
      const std::string space = data.substr(uuid_pos + 2, 36);
      // Extract the balanced JSON object.
      int depth = 0;
      bool quoted = false;
      bool escaped = false;
      std::size_t end = pos;
      for (; end < data.size(); ++end) {
        const char ch = data[end];
        if (quoted) {
          if (escaped) {
            escaped = false;
          } else if (ch == '\\') {
            escaped = true;
          } else if (ch == '"') {
            quoted = false;
          }
          continue;
        }
        if (ch == '"') {
          quoted = true;
        } else if (ch == '{' || ch == '[') {
          ++depth;
        } else if (ch == '}' || ch == ']') {
          --depth;
          if (depth == 0) {
            ++end;
            break;
          }
        }
      }
      if (depth != 0) {
        break;
      }
      const std::string json = data.substr(pos, end - pos);
      JsonParser* parser = json_parser_new();
      if (json_parser_load_from_data(parser, json.c_str(), static_cast<gssize>(json.size()),
                                     nullptr)) {
        JsonNode* root = json_parser_get_root(parser);
        JsonObject* object = (root != nullptr && JSON_NODE_HOLDS_OBJECT(root))
                                 ? json_node_get_object(root)
                                 : nullptr;
        JsonArray* cards =
            (object != nullptr && json_object_has_member(object, "Statscards"))
                ? json_object_get_array_member(object, "Statscards")
                : nullptr;
        UbisoftStats stats = result[space];
        int total_playtime = 0;
        int coop_playtime = 0;
        int solo_playtime = 0;
        int pvp_playtime = 0;
        int achievements = 0;
        bool achievements_known = false;
        for (guint index = 0; cards != nullptr && index < json_array_get_length(cards);
             ++index) {
          JsonObject* card = json_array_get_object_element(cards, index);
          if (card == nullptr) {
            continue;
          }
          const std::string name = json_string(card, "statName");
          const int value = stat_number(json_value_text(card, "value"));
          if (name == "TotalPlaytime") {
            total_playtime = value;
          } else if (name == "CoopGlobalPlaytime") {
            coop_playtime = value;
          } else if (name == "SoloGlobalPlaytime") {
            solo_playtime = value;
          } else if (name == "PvPGlobalPlaytime" || name == "PVPGlobalPlaytime") {
            pvp_playtime = value;
          } else if (name == "AchievementsUnlocked") {
            achievements = value;
            achievements_known = true;
          }
        }
        if (total_playtime == 0) {
          total_playtime = coop_playtime + solo_playtime + pvp_playtime;
        }
        stats.playtime_minutes = total_playtime / 60;
        stats.achievements_unlocked = achievements;
        stats.achievements_known = achievements_known;
        result[space] = stats;
      }
      g_object_unref(parser);
      pos = end;
    }
  }
  ubisoft_log("stats: " + std::to_string(result.size()) + " entries");
  return result;
}

bool UbisoftClient::fetch_remote_stats(const UbisoftSession& session,
                                       const std::string& space_id, UbisoftStats* stats,
                                       std::string* error) {
  if (stats == nullptr || session.ticket.empty() || session.user_id.empty() ||
      space_id.empty()) {
    return false;
  }
  guint status = 0;
  std::string response;
  const std::vector<std::pair<std::string, std::string>> headers = {
      {"Authorization", session_headers_ticket(session, "Ubi_v1")},
      {"Ubi-SessionId", session.session_id},
      {"Ubi-RequestedPlatformType", "uplay"},
      {"Ubi-LocaleCode", "en-US"},
  };
  const std::string url = std::string("https://") + kApiHost + "/v1/profiles/" +
                          session.user_id + "/statscard?spaceId=" + space_id;
  if (!http("GET", url, headers, "", &status, &response, error) || status != 200) {
    if (error != nullptr && error->empty()) {
      *error = "Ubisoft stats request failed (HTTP " + std::to_string(status) + ").";
    }
    return false;
  }
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, response.c_str(),
                                  static_cast<gssize>(response.size()), nullptr)) {
    g_object_unref(parser);
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  JsonArray* cards =
      (object != nullptr && json_object_has_member(object, "Statscards"))
          ? json_object_get_array_member(object, "Statscards")
          : ((root != nullptr && JSON_NODE_HOLDS_ARRAY(root)) ? json_node_get_array(root)
                                                              : nullptr);
  int total_playtime = 0;
  int coop_playtime = 0;
  int solo_playtime = 0;
  int pvp_playtime = 0;
  int achievements = 0;
  bool achievements_known = false;
  for (guint index = 0; cards != nullptr && index < json_array_get_length(cards); ++index) {
    JsonObject* card = json_array_get_object_element(cards, index);
    if (card == nullptr) {
      continue;
    }
    const std::string name = json_string(card, "statName");
    const int value = stat_number(json_value_text(card, "value"));
    if (name == "TotalPlaytime") {
      total_playtime = value;
    } else if (name == "CoopGlobalPlaytime") {
      coop_playtime = value;
    } else if (name == "SoloGlobalPlaytime") {
      solo_playtime = value;
    } else if (name == "PvPGlobalPlaytime" || name == "PVPGlobalPlaytime") {
      pvp_playtime = value;
    } else if (name == "AchievementsUnlocked") {
      achievements = value;
      achievements_known = true;
    }
  }
  g_object_unref(parser);
  if (total_playtime == 0) {
    total_playtime = coop_playtime + solo_playtime + pvp_playtime;
  }
  stats->playtime_minutes = total_playtime / 60;
  stats->achievements_unlocked = achievements;
  stats->achievements_known = achievements_known;
  return true;
}

int UbisoftClient::achievement_total(const std::string& prefix,
                                     const std::string& space_id) {
  const std::string launch = launch_id_for(prefix, space_id);
  if (launch.empty()) {
    return 0;
  }
  const std::filesystem::path directory =
      std::filesystem::path(prefix) /
      "drive_c/ProgramData/Ubisoft/Ubisoft Game Launcher/cache/achievements";
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error)) {
    return 0;
  }
  const std::string prefix_name = launch + "_";
  for (std::filesystem::directory_iterator it(
           directory, std::filesystem::directory_options::skip_permission_denied, error),
       end;
       !error && it != end; it.increment(error)) {
    std::error_code entry_error;
    if (!it->is_regular_file(entry_error)) {
      continue;
    }
    const std::string name = it->path().filename().string();
    if (name.rfind(prefix_name, 0) != 0) {
      continue;
    }
    struct archive* archive = archive_read_new();
    archive_read_support_filter_all(archive);
    archive_read_support_format_zip(archive);
    if (archive_read_open_filename(archive, it->path().c_str(), 10240) != ARCHIVE_OK) {
      archive_read_free(archive);
      continue;
    }
    int total = 0;
    struct archive_entry* entry = nullptr;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
      const char* pathname = archive_entry_pathname(entry);
      if (pathname == nullptr || std::string(pathname) != "achievements.dat") {
        archive_read_data_skip(archive);
        continue;
      }
      std::string data;
      char buffer[4096];
      la_ssize_t size = 0;
      while ((size = archive_read_data(archive, buffer, sizeof(buffer))) > 0) {
        data.append(buffer, static_cast<std::size_t>(size));
      }
      std::size_t pos = 0;
      while (pos < data.size() && static_cast<unsigned char>(data[pos]) == 0x0A) {
        ++pos;
        std::size_t length = 0;
        int shift = 0;
        while (pos < data.size()) {
          const unsigned char byte = static_cast<unsigned char>(data[pos++]);
          length |= static_cast<std::size_t>(byte & 0x7F) << shift;
          if ((byte & 0x80) == 0) {
            break;
          }
          shift += 7;
        }
        if (length == 0 || pos + length > data.size()) {
          break;
        }
        pos += length;
        ++total;
      }
      break;
    }
    archive_read_close(archive);
    archive_read_free(archive);
    if (total > 0) {
      return total;
    }
  }
  return 0;
}

void UbisoftClient::save_games_cache(const std::vector<UbisoftGame>& games) {
  if (games.empty()) {
    return;
  }
  JsonArray* array = json_array_new();
  for (const UbisoftGame& game : games) {
    JsonObject* object = json_object_new();
    json_object_set_string_member(object, "game_id", game.game_id.c_str());
    json_object_set_string_member(object, "space_id", game.space_id.c_str());
    json_object_set_string_member(object, "name", game.name.c_str());
    json_object_set_string_member(object, "cover_url", game.cover_url.c_str());
    json_object_set_string_member(object, "banner_url", game.banner_url.c_str());
    json_object_set_string_member(object, "background_url", game.background_url.c_str());
    json_object_set_string_member(object, "cover_path", game.cover_path.c_str());
    json_object_set_string_member(object, "hero_path", game.hero_path.c_str());
    json_object_set_int_member(object, "playtime_minutes", game.playtime_minutes);
    json_object_set_int_member(object, "achievements_unlocked", game.achievements_unlocked);
    json_object_set_int_member(object, "achievements_total", game.achievements_total);
    json_object_set_boolean_member(object, "stats_available", game.stats_available);
    json_object_set_boolean_member(object, "achievements_known", game.achievements_known);
    json_array_add_object_element(array, object);
  }
  JsonNode* root = json_node_alloc();
  json_node_init_array(root, array);
  JsonGenerator* generator = json_generator_new();
  json_generator_set_root(generator, root);
  gchar* data = json_generator_to_data(generator, nullptr);
  if (data != nullptr) {
    const std::string path = Glib::build_filename(cache_dir(), "ubisoft-library.json");
    const std::string temporary = path + ".tmp";
    GError* error = nullptr;
    g_mkdir_with_parents(cache_dir().c_str(), 0700);
    if (g_file_set_contents(temporary.c_str(), data, -1, &error)) {
      ::chmod(temporary.c_str(), 0600);
      if (g_rename(temporary.c_str(), path.c_str()) != 0) {
        g_unlink(temporary.c_str());
      }
    } else if (error != nullptr) {
      g_error_free(error);
    }
    g_free(data);
  }
  g_object_unref(generator);
  json_node_free(root);
}

std::vector<UbisoftGame> UbisoftClient::load_games_cache() {
  std::vector<UbisoftGame> games;
  const std::string path = Glib::build_filename(cache_dir(), "ubisoft-library.json");
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_file(parser, path.c_str(), nullptr)) {
    g_object_unref(parser);
    return games;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonArray* array =
      (root != nullptr && JSON_NODE_HOLDS_ARRAY(root)) ? json_node_get_array(root) : nullptr;
  for (guint index = 0; array != nullptr && index < json_array_get_length(array); ++index) {
    JsonObject* object = json_array_get_object_element(array, index);
    if (object == nullptr) {
      continue;
    }
    UbisoftGame game;
    game.game_id = json_string(object, "game_id");
    game.space_id = json_string(object, "space_id");
    game.name = json_string(object, "name");
    game.cover_url = json_string(object, "cover_url");
    game.banner_url = json_string(object, "banner_url");
    game.background_url = json_string(object, "background_url");
    game.cover_path = json_string(object, "cover_path");
    game.hero_path = json_string(object, "hero_path");
    game.playtime_minutes =
        static_cast<int>(json_object_get_int_member_with_default(object, "playtime_minutes", 0));
    game.achievements_unlocked = static_cast<int>(
        json_object_get_int_member_with_default(object, "achievements_unlocked", 0));
    game.achievements_total =
        static_cast<int>(json_object_get_int_member_with_default(object, "achievements_total", 0));
    game.stats_available = json_object_get_boolean_member_with_default(
        object, "stats_available", FALSE) != FALSE;
    game.achievements_known = json_object_get_boolean_member_with_default(
        object, "achievements_known", FALSE) != FALSE;
    if (!game.space_id.empty() && !game.name.empty()) {
      games.push_back(std::move(game));
    }
  }
  g_object_unref(parser);
  return games;
}

void UbisoftClient::clear_games_cache() {
  const std::string path = Glib::build_filename(cache_dir(), "ubisoft-library.json");
  g_unlink(path.c_str());
}

bool UbisoftClient::save_session(const UbisoftSession& session) {
  JsonObject* object = json_object_new();
  json_object_set_string_member(object, "ticket", session.ticket.c_str());
  json_object_set_string_member(object, "session_id", session.session_id.c_str());
  json_object_set_string_member(object, "remember_me_ticket",
                                session.remember_me_ticket.c_str());
  json_object_set_string_member(object, "user_id", session.user_id.c_str());
  json_object_set_string_member(object, "username", session.username.c_str());
  json_object_set_int_member(object, "refresh_time", session.refresh_time);
  JsonNode* root = json_node_alloc();
  json_node_init_object(root, object);
  JsonGenerator* generator = json_generator_new();
  json_generator_set_root(generator, root);
  gchar* data = json_generator_to_data(generator, nullptr);
  bool saved = false;
  if (data != nullptr) {
    std::string error;
    saved = SecretStore::store(kSecretKey, data, &error);
    g_free(data);
  }
  g_object_unref(generator);
  json_node_free(root);
  return saved;
}

std::optional<UbisoftSession> UbisoftClient::load_session() {
  const std::string stored = SecretStore::lookup(kSecretKey);
  if (stored.empty()) {
    return std::nullopt;
  }
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, stored.c_str(), static_cast<gssize>(stored.size()),
                                  nullptr)) {
    g_object_unref(parser);
    return std::nullopt;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  if (object == nullptr) {
    g_object_unref(parser);
    return std::nullopt;
  }
  UbisoftSession session;
  session.ticket = json_string(object, "ticket");
  session.session_id = json_string(object, "session_id");
  session.remember_me_ticket = json_string(object, "remember_me_ticket");
  session.user_id = json_string(object, "user_id");
  session.username = json_string(object, "username");
  session.refresh_time = json_object_get_int_member_with_default(object, "refresh_time", 0);
  g_object_unref(parser);
  if (session.ticket.empty()) {
    return std::nullopt;
  }
  return session;
}

void UbisoftClient::clear_session() { SecretStore::remove(kSecretKey); }

std::string ubisoft_launch_uri(const std::string& space_id) {
  if (space_id.empty()) {
    return {};
  }
  return "uplay://launch/" + space_id + "/0";
}

std::string ubisoft_install_uri(const std::string& space_id) {
  if (space_id.empty()) {
    return {};
  }
  return "uplay://install/" + space_id;
}

} // namespace gameslauncher
