#include "core/artwork.h"

#include "core/paths.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>
#include <optional>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace gameslauncher::artwork {
namespace {

constexpr gsize kMaxCoverBytes = 16 * 1024 * 1024;
constexpr guint kTimeoutSeconds = 10;

bool numeric_id(const std::string& value) {
  return !value.empty() && value.size() <= 32 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return std::isdigit(ch) != 0;
         });
}

std::string cover_dir() {
  return Glib::build_filename(cache_dir(), "covers");
}

std::string cached_path_for(const std::string& app_id, const std::string& extension) {
  return Glib::build_filename(cover_dir(), app_id + extension);
}

const char* image_extension(const guchar* data, gsize size) {
  if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
    return ".jpg";
  }
  if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
    return ".png";
  }
  if (size >= 12 && std::memcmp(data, "RIFF", 4) == 0 &&
      std::memcmp(data + 8, "WEBP", 4) == 0) {
    return ".webp";
  }
  return nullptr;
}

bool write_private(const std::string& path, const guchar* data, gsize size) {
  const std::string temporary = path + ".tmp";
  GError* error = nullptr;
  const bool written =
      g_file_set_contents(temporary.c_str(), reinterpret_cast<const char*>(data),
                          static_cast<gssize>(size), &error);
  if (!written) {
    if (error != nullptr) {
      g_warning("Could not write cover: %s", error->message);
      g_error_free(error);
    }
    g_unlink(temporary.c_str());
    return false;
  }
  ::chmod(temporary.c_str(), 0600);
  if (g_rename(temporary.c_str(), path.c_str()) != 0) {
    g_unlink(temporary.c_str());
    return false;
  }
  return true;
}

std::string hero_cache_dir() {
  return Glib::build_filename(cache_dir(), "heroes");
}

std::string hero_path_for(const std::string& app_id, const std::string& extension) {
  return Glib::build_filename(hero_cache_dir(), app_id + extension);
}

std::string http_get(const std::string& url, guint* status) {
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new("GET", url.c_str());
  std::string body;
  if (message != nullptr) {
    GBytes* bytes = soup_session_send_and_read(session, message, nullptr, nullptr);
    if (status != nullptr) {
      *status = soup_message_get_status(message);
    }
    if (bytes != nullptr) {
      gsize size = 0;
      const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
      if (data != nullptr) {
        body.assign(data, size);
      }
      g_bytes_unref(bytes);
    }
    g_object_unref(message);
  }
  g_object_unref(session);
  return body;
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

// Uses Steam's public store browse API, which returns the real asset paths.
// It needs no key and covers new releases whose plain asset filenames 404.
std::vector<std::string> steam_asset_urls(const std::string& app_id, bool hero) {
  std::vector<std::string> urls;
  const std::string base = "https://shared.steamstatic.com/store_item_assets/";
  const std::string payload =
      "{\"ids\":[{\"appid\":" + app_id +
      "}],\"context\":{\"language\":\"english\",\"country_code\":\"US\"},"
      "\"data_request\":{\"include_assets\":true}}";
  gchar* escaped = g_uri_escape_string(payload.c_str(), nullptr, false);
  guint status = 0;
  const std::string body =
      http_get("https://api.steampowered.com/IStoreBrowseService/GetItems/v1/?input_json=" +
                   std::string(escaped != nullptr ? escaped : ""),
               &status);
  g_free(escaped);
  if (status == 200 && !body.empty()) {
    JsonParser* parser = json_parser_new();
    if (json_parser_load_from_data(parser, body.c_str(),
                                   static_cast<gssize>(body.size()), nullptr)) {
      JsonNode* root = json_parser_get_root(parser);
      JsonObject* response =
          (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root)
                                                            : nullptr;
      JsonObject* items = nullptr;
      if (response != nullptr && json_object_has_member(response, "response")) {
        JsonObject* wrapper = json_object_get_object_member(response, "response");
        if (wrapper != nullptr && json_object_has_member(wrapper, "store_items")) {
          JsonArray* array = json_object_get_array_member(wrapper, "store_items");
          if (array != nullptr && json_array_get_length(array) > 0) {
            items = json_array_get_object_element(array, 0);
          }
        }
      }
      JsonObject* assets = nullptr;
      if (items != nullptr && json_object_has_member(items, "assets")) {
        assets = json_object_get_object_member(items, "assets");
      }
      const std::string format = json_string(assets, "asset_url_format");
      if (assets != nullptr && !format.empty()) {
        const char* keys[] = {hero ? "library_hero_2x" : "library_capsule_2x",
                              hero ? "library_hero" : "library_capsule",
                              "main_capsule_2x", "main_capsule", "header_2x", "header"};
        for (const char* key : keys) {
          const std::string filename = json_string(assets, key);
          if (filename.empty()) {
            continue;
          }
          std::string url = base + format;
          const std::string marker = "${FILENAME}";
          const std::size_t position = url.find(marker);
          if (position == std::string::npos) {
            continue;
          }
          url.replace(position, marker.size(), filename);
          urls.push_back(std::move(url));
        }
      }
    }
    g_object_unref(parser);
  }

  // Compatibility with older store items that still publish plain filenames.
  for (const char* filename : hero ? std::initializer_list<const char*>{
                                         "library_hero_2x.jpg", "library_hero.jpg",
                                         "header_2x.jpg", "header.jpg"}
                                   : std::initializer_list<const char*>{
                                         "library_600x900_2x.jpg", "library_600x900.jpg",
                                         "library_capsule.jpg", "header.jpg"}) {
    urls.push_back(base + app_id + "/" + filename);
  }
  return urls;
}

bool download_first(const std::vector<std::string>& urls, bool hero,
                    const std::string& app_id, std::string* out) {
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  bool saved = false;
  for (const std::string& url : urls) {
    SoupMessage* message = soup_message_new("GET", url.c_str());
    if (message == nullptr) {
      continue;
    }
    GBytes* bytes = soup_session_send_and_read(session, message, nullptr, nullptr);
    const guint status = soup_message_get_status(message);
    const gsize size = bytes != nullptr ? g_bytes_get_size(bytes) : 0;
    const guchar* data =
        bytes != nullptr ? static_cast<const guchar*>(g_bytes_get_data(bytes, nullptr)) : nullptr;
    const char* extension = data != nullptr ? image_extension(data, size) : nullptr;
    if (status == 200 && size > 0 && size <= kMaxCoverBytes && extension != nullptr) {
      const std::string path =
          hero ? hero_path_for(app_id, extension) : cached_path_for(app_id, extension);
      saved = write_private(path, data, size);
      if (saved && out != nullptr) {
        *out = path;
      }
    }
    if (bytes != nullptr) {
      g_bytes_unref(bytes);
    }
    g_object_unref(message);
    if (saved) {
      break;
    }
  }
  g_object_unref(session);
  return saved;
}

} // namespace

std::string cached_cover_path(const std::string& app_id) {
  if (!numeric_id(app_id)) {
    return {};
  }
  for (const char* extension : {".jpg", ".png", ".webp"}) {
    const std::string path = cached_path_for(app_id, extension);
    if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
      return path;
    }
  }
  return {};
}

bool fetch_steam_cover(const std::string& app_id, std::string* path, std::string* error) {
  if (path != nullptr) {
    path->clear();
  }
  if (!numeric_id(app_id)) {
    if (error != nullptr) {
      *error = "invalid app id";
    }
    return false;
  }
  if (!ensure_private_dir(cover_dir())) {
    if (error != nullptr) {
      *error = "could not create the artwork cache";
    }
    return false;
  }
  const std::string existing = cached_cover_path(app_id);
  if (!existing.empty()) {
    if (path != nullptr) {
      *path = existing;
    }
    return true;
  }
  std::string output;
  if (download_first(steam_asset_urls(app_id, false), false, app_id, &output)) {
    if (path != nullptr) {
      *path = output;
    }
    return true;
  }
  if (error != nullptr) {
    *error = "no cover was available";
  }
  return false;
}
std::string image_cache_dir() {
  return Glib::build_filename(cache_dir(), "images");
}

std::string url_hash(const std::string& url) {
  gchar* hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, url.c_str(), -1);
  std::string result = hash != nullptr ? hash : "";
  g_free(hash);
  return result;
}

bool allowed_image_url(const std::string& url) {
  if (url.size() > 2048) {
    return false;
  }
  for (const char* prefix : {"https://shared.fastly.steamstatic.com/",
                             "https://shared.steamstatic.com/",
                             "https://steamcdn-a.akamaihd.net/",
                             "https://media.steampowered.com/",
                             "https://cdn.akamai.steamstatic.com/",
                             "https://steamcommunity-a.akamaihd.net/",
                             "https://cdn1.epicgames.com/",
                             "https://cdn2.unrealengine.com/",
                             "https://launcher-public-service-prod06.ol.epicgames.com/"}) {
    if (url.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

std::string cached_image_path(const std::string& url) {
  if (!allowed_image_url(url)) {
    return {};
  }
  const std::string hash = url_hash(url);
  if (hash.empty()) {
    return {};
  }
  for (const char* extension : {".jpg", ".png", ".webp"}) {
    const std::string path = Glib::build_filename(image_cache_dir(), hash + extension);
    if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
      return path;
    }
  }
  return {};
}

std::string fetch_image(const std::string& url, std::string* error) {
  if (!allowed_image_url(url)) {
    if (error != nullptr) {
      *error = "image URL is not allowed";
    }
    return {};
  }
  const std::string existing = cached_image_path(url);
  if (!existing.empty()) {
    return existing;
  }
  if (!ensure_private_dir(image_cache_dir())) {
    if (error != nullptr) {
      *error = "could not create the image cache";
    }
    return {};
  }
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new("GET", url.c_str());
  if (message == nullptr) {
    g_object_unref(session);
    if (error != nullptr) {
      *error = "could not create the image request";
    }
    return {};
  }
  GError* request_error = nullptr;
  GBytes* bytes = soup_session_send_and_read(session, message, nullptr, &request_error);
  const guint status = soup_message_get_status(message);
  const gsize size = bytes != nullptr ? g_bytes_get_size(bytes) : 0;
  const guchar* data =
      bytes != nullptr ? static_cast<const guchar*>(g_bytes_get_data(bytes, nullptr)) : nullptr;
  const char* extension = data != nullptr ? image_extension(data, size) : nullptr;
  std::string result;
  if (request_error != nullptr) {
    if (error != nullptr) {
      *error = request_error->message;
    }
    g_error_free(request_error);
  } else if (status != 200 || size == 0 || size > 4 * 1024 * 1024 || extension == nullptr) {
    if (error != nullptr) {
      *error = "image request failed or returned invalid data";
    }
  } else {
    const std::string path =
        Glib::build_filename(image_cache_dir(), url_hash(url) + extension);
    if (write_private(path, data, size)) {
      result = path;
    } else if (error != nullptr) {
      *error = "could not save the image";
    }
  }
  if (bytes != nullptr) {
    g_bytes_unref(bytes);
  }
  g_object_unref(message);
  g_object_unref(session);
  return result;
}

namespace {

std::mutex g_store_mutex;

std::string store_map_path() {
  return Glib::build_filename(cache_dir(), "store-appids.json");
}

std::string normalize_key(const std::string& value) {
  std::string result;
  bool previous_space = false;
  for (unsigned char ch : value) {
    if (ch == '\'' || ch >= 0x80) {
      // Apostrophes are removed so "Tom Clancy's" matches "tom clancys".
      continue;
    }
    if (std::isalnum(ch) != 0) {
      result.push_back(static_cast<char>(std::tolower(ch)));
      previous_space = false;
    } else if (!previous_space && !result.empty()) {
      result.push_back(' ');
      previous_space = true;
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  const std::string prefix = "tom clancys ";
  if (result.rfind(prefix, 0) == 0) {
    result.erase(0, prefix.size());
  }
  return result;
}

std::vector<std::string> tokens(const std::string& key) {
  std::vector<std::string> result;
  std::istringstream stream(key);
  std::string word;
  while (stream >> word) {
    result.push_back(word);
  }
  return result;
}

double title_score(const std::string& query, const std::string& candidate) {
  const std::string left = normalize_key(query);
  const std::string right = normalize_key(candidate);
  if (left.empty() || right.empty()) {
    return 0.0;
  }
  if (left == right) {
    return 1.0;
  }
  const std::vector<std::string> a = tokens(left);
  const std::vector<std::string> b = tokens(right);
  std::size_t shared = 0;
  for (const std::string& word : a) {
    if (std::find(b.begin(), b.end(), word) != b.end()) {
      ++shared;
    }
  }
  const std::size_t total = a.size() + b.size() - shared;
  return total == 0 ? 0.0 : static_cast<double>(shared) / static_cast<double>(total);
}

std::optional<std::string> mapped_appid(const std::string& key) {
  std::lock_guard lock(g_store_mutex);
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_file(parser, store_map_path().c_str(), nullptr)) {
    g_object_unref(parser);
    return std::nullopt;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  if (object == nullptr || !json_object_has_member(object, key.c_str())) {
    g_object_unref(parser);
    return std::nullopt;
  }
  const char* value = json_object_get_string_member_with_default(object, key.c_str(), "");
  std::string result = value != nullptr ? value : "";
  g_object_unref(parser);
  return result;
}

void remember_appid(const std::string& key, const std::string& app_id) {
  std::lock_guard lock(g_store_mutex);
  if (!ensure_private_dir(cache_dir())) {
    return;
  }
  JsonParser* parser = json_parser_new();
  JsonNode* root = nullptr;
  bool loaded = json_parser_load_from_file(parser, store_map_path().c_str(), nullptr);
  if (loaded) {
    root = json_parser_get_root(parser);
  }
  if (root == nullptr || !JSON_NODE_HOLDS_OBJECT(root)) {
    if (loaded) {
      g_object_unref(parser);
    }
    parser = json_parser_new();
    root = json_node_alloc();
    json_node_init_object(root, json_object_new());
  }
  json_object_set_string_member(json_node_get_object(root), key.c_str(), app_id.c_str());

  JsonGenerator* generator = json_generator_new();
  json_generator_set_root(generator, root);
  gchar* data = json_generator_to_data(generator, nullptr);
  if (data != nullptr) {
    const std::string path = store_map_path();
    const std::string temporary = path + ".tmp";
    GError* error = nullptr;
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
  if (loaded) {
    g_object_unref(parser);
  } else {
    json_node_free(root);
    g_object_unref(parser);
  }
}

std::optional<std::string> search_steam_app_id(const std::string& title) {
  gchar* escaped = g_uri_escape_string(title.c_str(), nullptr, false);
  const std::string url =
      "https://store.steampowered.com/api/storesearch/?term=" +
      std::string(escaped != nullptr ? escaped : "") + "&l=english&cc=US";
  g_free(escaped);

  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new("GET", url.c_str());
  std::optional<std::string> result;
  if (message != nullptr) {
    GBytes* bytes = soup_session_send_and_read(session, message, nullptr, nullptr);
    const guint status = soup_message_get_status(message);
    if (status == 200 && bytes != nullptr) {
      gsize size = 0;
      const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
      JsonParser* parser = json_parser_new();
      if (data != nullptr &&
          json_parser_load_from_data(parser, data, static_cast<gssize>(size), nullptr)) {
        JsonNode* root = json_parser_get_root(parser);
        JsonObject* object = (root != nullptr && JSON_NODE_HOLDS_OBJECT(root))
                                 ? json_node_get_object(root)
                                 : nullptr;
        double best = 0.55;
        std::string best_id;
        if (object != nullptr && json_object_has_member(object, "items")) {
          JsonArray* items = json_object_get_array_member(object, "items");
          for (guint index = 0; items != nullptr && index < json_array_get_length(items);
               ++index) {
            JsonObject* item = json_array_get_object_element(items, index);
            if (item == nullptr) {
              continue;
            }
            const std::string name =
                json_object_get_string_member_with_default(item, "name", "");
            const double score = title_score(title, name);
            if (score > best) {
              best = score;
              best_id = std::to_string(json_object_get_int_member_with_default(item, "id", 0));
            }
          }
        }
        result = best_id;
      }
      g_object_unref(parser);
    }
    if (bytes != nullptr) {
      g_bytes_unref(bytes);
    }
    g_object_unref(message);
  }
  g_object_unref(session);
  return result;
}

std::string cached_hero_path(const std::string& app_id) {
  if (!numeric_id(app_id)) {
    return {};
  }
  for (const char* extension : {".jpg", ".png", ".webp"}) {
    const std::string path = hero_path_for(app_id, extension);
    if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
      return path;
    }
  }
  return {};
}

bool fetch_steam_hero(const std::string& app_id) {
  if (!numeric_id(app_id) || !ensure_private_dir(hero_cache_dir())) {
    return false;
  }
  if (!cached_hero_path(app_id).empty()) {
    return true;
  }
  return download_first(steam_asset_urls(app_id, true), true, app_id, nullptr);
}

} // namespace

bool ensure_store_artwork(const std::string& title, std::string* error) {
  if (title.empty()) {
    if (error != nullptr) {
      *error = "empty title";
    }
    return false;
  }
  const std::string key = normalize_key(title);
  const std::optional<std::string> mapped = mapped_appid(key);
  std::string app_id;
  if (mapped.has_value() && !mapped->empty()) {
    app_id = *mapped;
  } else {
    const std::optional<std::string> found = search_steam_app_id(title);
    if (!found.has_value()) {
      if (error != nullptr) {
        *error = "Steam search is unavailable";
      }
      return false;
    }
    app_id = *found;
    if (!app_id.empty()) {
      remember_appid(key, app_id);
    }
  }
  if (app_id.empty()) {
    if (error != nullptr) {
      *error = "no Steam match for this title";
    }
    return false;
  }
  std::string cover;
  if (!fetch_steam_cover(app_id, &cover, nullptr) || cover.empty()) {
    if (error != nullptr) {
      *error = "Steam has no cover for this title";
    }
    return false;
  }
  fetch_steam_hero(app_id);
  return true;
}


std::string cached_store_cover(const std::string& title) {
  const std::string key = normalize_key(title);
  const std::optional<std::string> mapped = mapped_appid(key);
  if (mapped.has_value() && !mapped->empty()) {
    return cached_cover_path(*mapped);
  }
  return {};
}

std::string cached_store_hero(const std::string& title) {
  const std::string key = normalize_key(title);
  const std::optional<std::string> mapped = mapped_appid(key);
  if (mapped.has_value() && !mapped->empty()) {
    return cached_hero_path(*mapped);
  }
  return {};
}


} // namespace gameslauncher::artwork
