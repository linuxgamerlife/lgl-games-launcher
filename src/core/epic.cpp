#include "core/epic.h"

#include "core/secret_store.h"

#include "core/paths.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <utility>

namespace gameslauncher {

void epic_log(const std::string& message) {
  const std::string path = Glib::build_filename(cache_dir(), "epic.log");
  g_mkdir_with_parents(cache_dir().c_str(), 0700);
  FILE* file = g_fopen(path.c_str(), "a");
  if (file != nullptr) {
    std::fprintf(file, "%s\n", message.c_str());
    std::fclose(file);
  }
}

namespace {

constexpr const char* kClientId = "34a02cf8f4414e29b15921876da36f9a";
constexpr const char* kClientSecret = "daafbccc737745039dffe53d94fc76cf";
constexpr const char* kOauthHost = "account-public-service-prod03.ol.epicgames.com";
constexpr const char* kEntitlementsHost = "entitlement-public-service-prod08.ol.epicgames.com";
constexpr const char* kCatalogHost = "catalog-public-service-prod06.ol.epicgames.com";
constexpr const char* kUserAgent =
    "UELauncher/11.0.1-14907503+++Portal+Release-Live Windows/10.0.19041.1.256.64bit";
constexpr guint kTimeoutSeconds = 20;
constexpr gsize kMaxResponseBytes = 8 * 1024 * 1024;
constexpr const char* kSecretKey = "epic-session";

std::string form_value(const std::string& value) {
  gchar* escaped = g_uri_escape_string(value.c_str(), nullptr, true);
  std::string result = escaped != nullptr ? escaped : "";
  g_free(escaped);
  return result;
}

std::string basic_auth() {
  const std::string joined = std::string(kClientId) + ":" + kClientSecret;
  gchar* encoded =
      g_base64_encode(reinterpret_cast<const guchar*>(joined.data()), joined.size());
  std::string result = encoded != nullptr ? encoded : "";
  g_free(encoded);
  return "basic " + result;
}

bool http(const std::string& method, const std::string& url,
          const std::vector<std::pair<std::string, std::string>>& headers,
          const std::string& body, guint* status, std::string* response,
          std::string* error);
std::string json_string(JsonObject* object, const char* member);
long long iso_seconds(const std::string& value);

std::mutex g_client_token_mutex;
std::string g_client_token;
long long g_client_token_expires = 0;

std::string client_token() {
  std::lock_guard lock(g_client_token_mutex);
  const long long now = static_cast<long long>(g_get_real_time() / 1000000);
  if (!g_client_token.empty() && now + 60 < g_client_token_expires) {
    return g_client_token;
  }
  guint status = 0;
  std::string response;
  if (!http("POST", std::string("https://") + kOauthHost + "/account/api/oauth/token",
            {{"Authorization", basic_auth()}},
            "grant_type=client_credentials&token_type=eg1", &status, &response, nullptr) ||
      status != 200) {
    epic_log("client_token: HTTP " + std::to_string(status));
    return {};
  }
  JsonParser* parser = json_parser_new();
  std::string token;
  if (json_parser_load_from_data(parser, response.c_str(),
                                 static_cast<gssize>(response.size()), nullptr)) {
    JsonNode* root = json_parser_get_root(parser);
    JsonObject* object =
        (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
    token = json_string(object, "access_token");
    g_client_token_expires = iso_seconds(json_string(object, "expires_at"));
  }
  g_object_unref(parser);
  if (!token.empty()) {
    g_client_token = token;
  }
  epic_log(token.empty() ? "client_token: failed" : "client_token: ok");
  return token;
}

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
      *error = "Could not create the Epic request.";
    }
    return false;
  }
  SoupMessageHeaders* request_headers = soup_message_get_request_headers(message);
  soup_message_headers_append(request_headers, "User-Agent", kUserAgent);
  for (const auto& [name, value] : headers) {
    soup_message_headers_append(request_headers, name.c_str(), value.c_str());
  }
  if (!body.empty()) {
    GBytes* bytes = g_bytes_new(body.data(), body.size());
    soup_message_set_request_body_from_bytes(
        message, "application/x-www-form-urlencoded", bytes);
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
      *error = "The Epic response was too large.";
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

bool parse_session(const std::string& body, EpicSession* session, std::string* error) {
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body.c_str(), static_cast<gssize>(body.size()),
                                  nullptr)) {
    g_object_unref(parser);
    if (error != nullptr) {
      *error = "Epic returned an invalid response.";
    }
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  const std::string error_code = json_string(object, "errorCode");
  if (!error_code.empty()) {
    if (error != nullptr) {
      *error = json_string(object, "errorMessage").empty()
                   ? error_code
                   : json_string(object, "errorMessage");
    }
    g_object_unref(parser);
    return false;
  }
  const std::string access = json_string(object, "access_token");
  const std::string refresh = json_string(object, "refresh_token");
  const std::string account = json_string(object, "account_id");
  if (access.empty() || refresh.empty() || account.empty()) {
    if (error != nullptr) {
      *error = "Epic did not return a complete session.";
    }
    g_object_unref(parser);
    return false;
  }
  session->access_token = access;
  session->refresh_token = refresh;
  session->account_id = account;
  const std::string display = json_string(object, "displayName");
  if (!display.empty()) {
    session->display_name = display;
  }
  session->expires_at = iso_seconds(json_string(object, "expires_at"));
  g_object_unref(parser);
  return true;
}

bool token_request(const std::string& form, EpicSession* session, std::string* error) {
  guint status = 0;
  std::string response;
  if (!http("POST", std::string("https://") + kOauthHost + "/account/api/oauth/token",
            {{"Authorization", basic_auth()}}, form, &status, &response, error)) {
    return false;
  }
  if (status != 200) {
    std::string message;
    parse_session(response, session, &message);
    if (error != nullptr) {
      *error = message.empty() ? "Epic login failed (HTTP " + std::to_string(status) + ")."
                               : message;
    }
    return false;
  }
  return parse_session(response, session, error);
}

std::string cover_from_images(JsonObject* object) {
  if (object == nullptr || !json_object_has_member(object, "keyImages")) {
    return {};
  }
  JsonArray* images = json_object_get_array_member(object, "keyImages");
  for (const char* wanted : {"DieselGameBoxTall", "OfferImageTall", "Thumbnail",
                             "DieselGameBox"}) {
    for (guint index = 0; images != nullptr && index < json_array_get_length(images);
         ++index) {
      JsonObject* image = json_array_get_object_element(images, index);
      if (image != nullptr && json_string(image, "type") == wanted) {
        return json_string(image, "url");
      }
    }
  }
  return {};
}

bool has_category(JsonObject* object, const char* wanted) {
  if (object == nullptr || !json_object_has_member(object, "categories")) {
    return false;
  }
  JsonArray* categories = json_object_get_array_member(object, "categories");
  for (guint index = 0; categories != nullptr && index < json_array_get_length(categories);
       ++index) {
    JsonObject* category = json_array_get_object_element(categories, index);
    if (category != nullptr && json_string(category, "path") == wanted) {
      return true;
    }
  }
  return false;
}

std::string app_name_for(JsonObject* object) {
  if (object == nullptr || !json_object_has_member(object, "releaseInfo")) {
    return {};
  }
  JsonArray* releases = json_object_get_array_member(object, "releaseInfo");
  for (guint index = 0; releases != nullptr && index < json_array_get_length(releases);
       ++index) {
    JsonObject* release = json_array_get_object_element(releases, index);
    if (release == nullptr) {
      continue;
    }
    const std::string app = json_string(release, "appId");
    if (!app.empty()) {
      return app;
    }
  }
  return {};
}

bool parse_catalog(const std::string& body, const std::string& catalog_id, EpicGame* game) {
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body.c_str(), static_cast<gssize>(body.size()),
                                  nullptr)) {
    g_object_unref(parser);
    return false;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* wrapper =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  JsonObject* item = nullptr;
  if (wrapper != nullptr && json_object_has_member(wrapper, catalog_id.c_str())) {
    item = json_object_get_object_member(wrapper, catalog_id.c_str());
  }
  bool ok = false;
  if (item != nullptr) {
    const std::string title = json_string(item, "title");
    const std::string app = app_name_for(item);
    const bool is_game = has_category(item, "games") || has_category(item, "applications");
    const bool is_extra = has_category(item, "addons") || has_category(item, "digitalextras") ||
                          has_category(item, "plugins") || has_category(item, "enginecontent");
    if (!title.empty() && !app.empty() && is_game && !is_extra) {
      game->title = title;
      game->app_name = app;
      game->namespace_id = json_string(item, "namespace");
      game->catalog_id = catalog_id;
      game->cover_url = cover_from_images(item);
      ok = true;
    }
  }
  g_object_unref(parser);
  return ok;
}

} // namespace

std::string EpicClient::login_url() {
  const std::string redirect =
      std::string("https://www.epicgames.com/id/api/redirect?clientId=") + kClientId +
      "&responseType=code";
  return "https://www.epicgames.com/id/login?redirectUrl=" + form_value(redirect);
}

bool EpicClient::exchange_code(const std::string& code, EpicSession* session,
                               std::string* error) {
  epic_log("exchange_code: start");
  if (session == nullptr || code.empty()) {
    if (error != nullptr) {
      *error = "Epic returned an empty authorization code.";
    }
    return false;
  }
  const std::string form = "grant_type=authorization_code&code=" + form_value(code) +
                           "&token_type=eg1";
  const bool ok = token_request(form, session, error);
  epic_log(ok ? "exchange_code: ok"
              : "exchange_code: failed " + (error != nullptr ? *error : std::string()));
  return ok;
}

bool EpicClient::refresh(EpicSession* session, std::string* error) {
  epic_log("refresh: start");
  if (session == nullptr || session->refresh_token.empty()) {
    if (error != nullptr) {
      *error = "No Epic session is stored.";
    }
    return false;
  }
  const std::string form =
      "grant_type=refresh_token&refresh_token=" + form_value(session->refresh_token) +
      "&token_type=eg1";
  const bool ok = token_request(form, session, error);
  epic_log(ok ? "refresh: ok" : "refresh: failed " + (error != nullptr ? *error : std::string()));
  return ok;
}

bool EpicClient::fetch_owned_games(const EpicSession& session,
                                   std::vector<EpicGame>* games, std::string* error) {
  if (games == nullptr || session.access_token.empty() || session.account_id.empty()) {
    if (error != nullptr) {
      *error = "Epic is not connected.";
    }
    return false;
  }
  games->clear();
  std::vector<std::pair<std::string, std::string>> items;
  std::size_t start = 0;
  while (start < 10000) {
    guint status = 0;
    std::string response;
    const std::string url = std::string("https://") + kEntitlementsHost +
                            "/entitlement/api/account/" + session.account_id +
                            "/entitlements?start=" + std::to_string(start) + "&count=1000";
    if (!http("GET", url, {{"Authorization", "bearer " + session.access_token}}, "", &status,
              &response, error)) {
      epic_log("entitlements: transport failure " +
               (error != nullptr ? *error : std::string()));
      return false;
    }
    if (status != 200) {
      epic_log("entitlements: HTTP " + std::to_string(status) + " (continuing)");
      break;
    }
    JsonParser* parser = json_parser_new();
    if (!json_parser_load_from_data(parser, response.c_str(),
                                    static_cast<gssize>(response.size()), nullptr)) {
      g_object_unref(parser);
      if (error != nullptr) {
        *error = "Epic returned an invalid library response.";
      }
      return false;
    }
    JsonNode* root = json_parser_get_root(parser);
    JsonArray* array =
        (root != nullptr && JSON_NODE_HOLDS_ARRAY(root)) ? json_node_get_array(root) : nullptr;
    const guint length = array != nullptr ? json_array_get_length(array) : 0;
    for (guint index = 0; index < length; ++index) {
      JsonObject* entitlement = json_array_get_object_element(array, index);
      if (entitlement == nullptr) {
        continue;
      }
      const std::string catalog = json_string(entitlement, "catalogItemId");
      const std::string namespace_id = json_string(entitlement, "namespace");
      if (!catalog.empty() && !namespace_id.empty()) {
        items.emplace_back(namespace_id, catalog);
      }
    }
    g_object_unref(parser);
    epic_log("entitlements: page with " + std::to_string(length) + " entries");
    if (length < 1000) {
      break;
    }
    start += length;
  }

  epic_log("entitlements: unique catalog items " + std::to_string(items.size()));

  // The launcher assets endpoint is a second source of owned, launchable
  // games. Some accounts return an empty entitlements list but still have
  // assets, so merge both before resolving catalog metadata.
  {
    guint status = 0;
    std::string response;
    const std::string url =
        "https://launcher-public-service-prod06.ol.epicgames.com/launcher/api/public/"
        "assets/Windows?label=Live";
    if (http("GET", url, {{"Authorization", "bearer " + session.access_token}}, "", &status,
             &response, nullptr) &&
        status == 200) {
      JsonParser* parser = json_parser_new();
      if (json_parser_load_from_data(parser, response.c_str(),
                                     static_cast<gssize>(response.size()), nullptr)) {
        JsonNode* root = json_parser_get_root(parser);
        JsonArray* assets =
            (root != nullptr && JSON_NODE_HOLDS_ARRAY(root)) ? json_node_get_array(root)
                                                             : nullptr;
        for (guint index = 0; assets != nullptr && index < json_array_get_length(assets);
             ++index) {
          JsonObject* asset = json_array_get_object_element(assets, index);
          if (asset == nullptr) {
            continue;
          }
          const std::string catalog = json_string(asset, "catalogItemId");
          const std::string namespace_id = json_string(asset, "namespace");
          if (!catalog.empty() && !namespace_id.empty()) {
            items.emplace_back(namespace_id, catalog);
          }
        }
        epic_log("assets: HTTP 200, entries " +
                 std::to_string(assets != nullptr ? json_array_get_length(assets) : 0));
      }
      g_object_unref(parser);
    } else {
      epic_log("assets: HTTP " + std::to_string(status));
    }
  }

  std::sort(items.begin(), items.end());
  items.erase(std::unique(items.begin(), items.end()), items.end());
  if (items.empty()) {
    if (error != nullptr) {
      *error = "Epic returned no owned games for this account.";
    }
    epic_log("library: no catalog items after entitlements/assets");
    return false;
  }
  std::size_t catalog_failures = 0;
  std::size_t catalog_skipped = 0;
  for (const auto& [namespace_id, catalog_id] : items) {
    guint status = 0;
    std::string response;
    const std::string url = std::string("https://") + kCatalogHost +
                            "/catalog/api/shared/namespace/" + namespace_id +
                            "/bulk/items?id=" + catalog_id +
                            "&includeDLCDetails=false&includeMainGameDetails=true"
                            "&country=US&locale=en-US";
    if (!http("GET", url, {{"Authorization", "bearer " + session.access_token}}, "", &status,
              &response, nullptr) ||
        status != 200) {
      const std::string fallback = client_token();
      if (!fallback.empty()) {
        http("GET", url, {{"Authorization", "bearer " + fallback}}, "", &status, &response,
             nullptr);
      }
    }
    if (status != 200) {
      ++catalog_failures;
      epic_log("catalog: HTTP " + std::to_string(status) + " for " + catalog_id);
      continue;
    }
    EpicGame game;
    if (parse_catalog(response, catalog_id, &game)) {
      if (game.namespace_id.empty()) {
        game.namespace_id = namespace_id;
      }
      games->push_back(std::move(game));
    } else {
      ++catalog_skipped;
    }
    g_usleep(50000);
  }
  epic_log("catalog: kept " + std::to_string(games->size()) + ", skipped " +
           std::to_string(catalog_skipped) + ", failures " +
           std::to_string(catalog_failures));
  return true;
}

bool EpicClient::save_session(const EpicSession& session) {
  JsonObject* object = json_object_new();
  json_object_set_string_member(object, "access_token", session.access_token.c_str());
  json_object_set_string_member(object, "refresh_token", session.refresh_token.c_str());
  json_object_set_string_member(object, "account_id", session.account_id.c_str());
  json_object_set_string_member(object, "display_name", session.display_name.c_str());
  json_object_set_int_member(object, "expires_at", session.expires_at);
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

std::optional<EpicSession> EpicClient::load_session() {
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
  EpicSession session;
  session.access_token = json_string(object, "access_token");
  session.refresh_token = json_string(object, "refresh_token");
  session.account_id = json_string(object, "account_id");
  session.display_name = json_string(object, "display_name");
  session.expires_at = json_object_get_int_member_with_default(object, "expires_at", 0);
  g_object_unref(parser);
  if (session.refresh_token.empty() || session.account_id.empty()) {
    return std::nullopt;
  }
  return session;
}

void EpicClient::clear_session() { SecretStore::remove(kSecretKey); }

} // namespace gameslauncher
