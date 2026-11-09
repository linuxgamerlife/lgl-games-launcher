#include "core/steam_achievements.h"

#include <glib.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace gameslauncher {
namespace {

constexpr gsize kMaxXmlBytes = 8 * 1024 * 1024;
constexpr guint kTimeoutSeconds = 15;

bool numeric(const std::string& value) {
  return !value.empty() && value.size() <= 32 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) {
           return std::isdigit(ch) != 0;
         });
}

std::string child_text(xmlNode* parent, const char* name) {
  for (xmlNode* node = parent->children; node != nullptr; node = node->next) {
    if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, BAD_CAST name) != 0) {
      continue;
    }
    xmlChar* content = xmlNodeGetContent(node);
    if (content == nullptr) {
      return {};
    }
    std::string result(reinterpret_cast<const char*>(content));
    xmlFree(content);
    return result;
  }
  return {};
}

} // namespace

SteamAchievementsResult fetch_steam_achievements(const std::string& steam_id,
                                                 const std::string& app_id) {
  SteamAchievementsResult result;
  if (!numeric(steam_id) || !numeric(app_id)) {
    result.error = "No Steam profile is available.";
    return result;
  }

  const std::string url = "https://steamcommunity.com/profiles/" + steam_id + "/stats/" +
                          app_id + "/achievements/?xml=1&l=english";
  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  SoupMessage* message = soup_message_new("GET", url.c_str());
  if (message == nullptr) {
    g_object_unref(session);
    result.error = "Could not create the Steam request.";
    return result;
  }
  GError* request_error = nullptr;
  GBytes* bytes = soup_session_send_and_read(session, message, nullptr, &request_error);
  const guint status = soup_message_get_status(message);
  if (request_error != nullptr) {
    result.error = request_error->message;
    g_error_free(request_error);
  } else if (status != 200) {
    result.error = status == 403 || status == 500
                       ? "Steam profile or game statistics are private."
                       : "Steam returned HTTP " + std::to_string(status) + ".";
  } else if (bytes == nullptr || g_bytes_get_size(bytes) == 0 ||
             g_bytes_get_size(bytes) > kMaxXmlBytes) {
    result.error = "Steam returned an empty or oversized achievement list.";
  } else {
    gsize size = 0;
    const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
    xmlDoc* document = xmlReadMemory(data, static_cast<int>(size), "steam.xml", nullptr,
                                     XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (document == nullptr) {
      result.error = "Steam returned malformed achievement data.";
    } else {
      xmlNode* root = xmlDocGetRootElement(document);
      xmlNode* achievements = nullptr;
      if (root != nullptr) {
        for (xmlNode* node = root->children; node != nullptr; node = node->next) {
          if (node->type == XML_ELEMENT_NODE &&
              xmlStrcmp(node->name, BAD_CAST "achievements") == 0) {
            achievements = node;
            break;
          }
        }
      }
      if (achievements != nullptr) {
        for (xmlNode* node = achievements->children; node != nullptr; node = node->next) {
          if (node->type != XML_ELEMENT_NODE ||
              xmlStrcmp(node->name, BAD_CAST "achievement") != 0) {
            continue;
          }
          SteamAchievement achievement;
          achievement.api_name = child_text(node, "apiname");
          achievement.name = child_text(node, "name");
          achievement.description = child_text(node, "description");
          achievement.icon_url = child_text(node, "iconClosed");
          const std::string open_icon = child_text(node, "iconOpen");
          const std::string timestamp = child_text(node, "unlockTimestamp");
          if (!timestamp.empty()) {
            char* end = nullptr;
            achievement.unlock_time = std::strtoll(timestamp.c_str(), &end, 10);
            achievement.unlocked = end != timestamp.c_str() && achievement.unlock_time > 0;
          }
          if (achievement.unlocked && !open_icon.empty()) {
            achievement.icon_url = open_icon;
          }
          if (achievement.name.empty()) {
            achievement.name = "Hidden achievement";
            achievement.hidden = true;
          }
          if (achievement.description.empty()) {
            achievement.description = "Unlock to reveal details";
            achievement.hidden = true;
          }
          if (!achievement.api_name.empty()) {
            result.achievements.push_back(std::move(achievement));
          }
        }
        result.available = true;
        std::stable_sort(result.achievements.begin(), result.achievements.end(),
                         [](const SteamAchievement& first, const SteamAchievement& second) {
                           if (first.unlocked != second.unlocked) {
                             return first.unlocked;
                           }
                           return first.name < second.name;
                         });
      }
      xmlFreeDoc(document);
    }
  }

  if (bytes != nullptr) {
    g_bytes_unref(bytes);
  }
  g_object_unref(message);
  g_object_unref(session);
  return result;
}

} // namespace gameslauncher
