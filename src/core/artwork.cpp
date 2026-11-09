#include "core/artwork.h"

#include "core/paths.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <libsoup/soup.h>

#include <algorithm>
#include <cctype>
#include <cstring>
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

  SoupSession* session = soup_session_new();
  g_object_set(G_OBJECT(session), "timeout", kTimeoutSeconds, nullptr);
  std::string last_error = "no cover was available";
  for (const char* filename : {"library_600x900_2x.jpg", "library_600x900.jpg"}) {
    const std::string url =
        "https://shared.steamstatic.com/store_item_assets/steam/apps/" + app_id + "/" +
        filename;
    SoupMessage* message = soup_message_new("GET", url.c_str());
    if (message == nullptr) {
      last_error = "could not create the artwork request";
      continue;
    }
    GError* request_error = nullptr;
    GBytes* bytes = soup_session_send_and_read(session, message, nullptr, &request_error);
    const guint status = soup_message_get_status(message);
    if (request_error != nullptr) {
      last_error = request_error->message;
      g_error_free(request_error);
      g_object_unref(message);
      continue;
    }
    const gsize size = bytes != nullptr ? g_bytes_get_size(bytes) : 0;
    const guchar* data =
        bytes != nullptr ? static_cast<const guchar*>(g_bytes_get_data(bytes, nullptr)) : nullptr;
    const char* extension = data != nullptr ? image_extension(data, size) : nullptr;
    if (status == 200 && size > 0 && size <= kMaxCoverBytes && extension != nullptr) {
      const std::string output = cached_path_for(app_id, extension);
      const bool saved = write_private(output, data, size);
      if (saved) {
        if (path != nullptr) {
          *path = output;
        }
        g_bytes_unref(bytes);
        g_object_unref(message);
        g_object_unref(session);
        return true;
      }
      last_error = "could not save the cover";
    } else if (status != 200) {
      last_error = "Steam returned HTTP " + std::to_string(status);
    } else {
      last_error = "Steam returned an invalid or oversized image";
    }
    if (bytes != nullptr) {
      g_bytes_unref(bytes);
    }
    g_object_unref(message);
  }
  g_object_unref(session);
  if (error != nullptr) {
    *error = last_error;
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
                             "https://steamcommunity-a.akamaihd.net/"}) {
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

} // namespace gameslauncher::artwork
