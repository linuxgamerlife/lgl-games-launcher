#include "core/faugus.h"

#include <glib.h>
#include <glibmm/miscutils.h>
#include <json-glib/json-glib.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace gameslauncher {
namespace {

constexpr std::size_t kMaxGameIdLength = 256;

bool usable_gameid(const std::string& value) {
  if (value.empty() || value.size() > kMaxGameIdLength) {
    return false;
  }
  return std::none_of(value.begin(), value.end(), [](unsigned char ch) {
    return std::iscntrl(ch) != 0;
  });
}

std::string json_string(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node)) {
    return {};
  }
  const GType type = json_node_get_value_type(node);
  if (type != G_TYPE_STRING) {
    return {};
  }
  const char* value = json_node_get_string(node);
  return value != nullptr ? value : "";
}

long long json_integer(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return 0;
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node)) {
    return 0;
  }
  const GType type = json_node_get_value_type(node);
  if (type == G_TYPE_INT64 || type == G_TYPE_INT) {
    return static_cast<long long>(json_node_get_int(node));
  }
  if (type == G_TYPE_DOUBLE) {
    return static_cast<long long>(json_node_get_double(node));
  }
  return 0;
}

long long iso_timestamp(const std::string& value) {
  if (value.empty()) {
    return 0;
  }
  GTimeZone* zone = g_time_zone_new_local();
  GDateTime* parsed = g_date_time_new_from_iso8601(value.c_str(), zone);
  g_time_zone_unref(zone);
  if (parsed == nullptr) {
    return 0;
  }
  const long long seconds = static_cast<long long>(g_date_time_to_unix(parsed));
  g_date_time_unref(parsed);
  return seconds;
}

bool regular_file(const std::string& path) {
  return !path.empty() && std::filesystem::is_regular_file(path);
}

std::string first_existing(const std::vector<std::string>& candidates) {
  for (const std::string& candidate : candidates) {
    if (regular_file(candidate)) {
      return candidate;
    }
  }
  return {};
}

std::string cover_for(const std::string& covers_dir, const std::string& gameid,
                      const std::string& declared) {
  std::vector<std::string> candidates;
  if (!declared.empty()) {
    candidates.push_back(declared);
  }
  for (const char* extension : {".png", ".jpg", ".jpeg", ".webp"}) {
    candidates.push_back(Glib::build_filename(covers_dir, gameid + extension));
  }
  return first_existing(candidates);
}

} // namespace

std::string FaugusScanner::games_path() {
  const std::string native =
      Glib::build_filename(Glib::get_user_data_dir(), "faugus-launcher", "games.json");
  if (regular_file(native)) {
    return native;
  }
  const std::string flatpak = Glib::build_filename(
      Glib::get_home_dir(), ".var", "app", "io.github.Faugus.faugus-launcher", "data",
      "faugus-launcher", "games.json");
  if (regular_file(flatpak)) {
    return flatpak;
  }
  return native;
}

std::string FaugusScanner::detect_source(const std::string& gameid,
                                         const std::string& title,
                                         const std::string& path) {
  std::string haystack = gameid + " " + title + " " + path;
  std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (haystack.find("ubisoft") != std::string::npos) {
    return "Ubisoft";
  }
  if (haystack.find("epic") != std::string::npos) {
    return "Epic";
  }
  if (haystack.find("ea app") != std::string::npos ||
      haystack.find("ea-app") != std::string::npos ||
      haystack.find("ea desktop") != std::string::npos ||
      haystack.find("ealauncher") != std::string::npos ||
      haystack.find("electronic arts") != std::string::npos) {
    return "EA";
  }
  return "Faugus";
}

FaugusScanResult FaugusScanner::scan() {
  FaugusScanResult result;
  const std::string path = games_path();
  result.flatpak = path.find("/.var/app/io.github.Faugus.faugus-launcher/") !=
                   std::string::npos;
  if (!regular_file(path)) {
    return result;
  }
  result.available = true;

  GError* error = nullptr;
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_file(parser, path.c_str(), &error)) {
    result.warnings.push_back(std::string("Faugus library could not be read: ") +
                              (error != nullptr ? error->message : "unknown error"));
    if (error != nullptr) {
      g_error_free(error);
    }
    g_object_unref(parser);
    return result;
  }

  JsonNode* root = json_parser_get_root(parser);
  JsonArray* array =
      (root != nullptr && JSON_NODE_HOLDS_ARRAY(root)) ? json_node_get_array(root) : nullptr;
  const std::string data_dir =
      Glib::build_filename(Glib::get_user_data_dir(), "faugus-launcher");
  const std::string covers_dir = Glib::build_filename(data_dir, "covers");

  for (guint index = 0; array != nullptr && index < json_array_get_length(array); ++index) {
    JsonObject* object = json_array_get_object_element(array, index);
    if (object == nullptr) {
      continue;
    }
    FaugusGame game;
    game.gameid = json_string(object, "gameid");
    if (!usable_gameid(game.gameid)) {
      continue;
    }
    game.title = json_string(object, "title");
    if (game.title.empty()) {
      game.title = game.gameid;
    }
    game.path = json_string(object, "path");
    game.prefix = json_string(object, "prefix");
    game.icon_path = json_string(object, "icon");
    if (!regular_file(game.icon_path)) {
      game.icon_path = first_existing(
          {Glib::build_filename(data_dir, "icons", game.gameid + ".png")});
    }
    game.cover_path =
        cover_for(covers_dir, game.gameid, json_string(object, "cover"));
    if (game.cover_path.empty()) {
      game.cover_path = game.icon_path;
    }
    game.source = detect_source(game.gameid, game.title, game.path);
    game.runner = json_string(object, "runner");
    game.launch_arguments = json_string(object, "launch_arguments");

    const long long playtime_seconds = json_integer(object, "playtime");
    game.playtime_minutes =
        static_cast<int>(std::clamp<long long>(playtime_seconds / 60, 0, 100000000));
    game.last_played = iso_timestamp(json_string(object, "last_played"));
    result.games.push_back(std::move(game));
  }
  g_object_unref(parser);
  return result;
}

} // namespace gameslauncher
