#pragma once

#include <string>

namespace gameslauncher::artwork {

// A locally cached Steam cover for an app, or empty when none is available.
[[nodiscard]] std::string cached_cover_path(const std::string& app_id);

// Fetches the portrait cover from Steam's public CDN and caches it. The app
// id must be numeric. This is best effort: a failure returns false and leaves
// no partial file behind. It never sends anything except the app id in the URL.
bool fetch_steam_cover(const std::string& app_id, std::string* path, std::string* error);

// Ensures a store game has cover and hero art. It resolves the title through
// Steam's public store search, then uses the public store browse API to fetch
// the real hashed cover and hero assets. No generated artwork is used.
bool ensure_store_artwork(const std::string& title, std::string* error = nullptr);
[[nodiscard]] std::string cached_store_cover(const std::string& title);
[[nodiscard]] std::string cached_store_hero(const std::string& title);

// Fetches a Steam-community image (achievement icons) from an allow-listed
// HTTPS host and caches it by URL hash. Returns an empty path on failure.
std::string fetch_image(const std::string& url, std::string* error = nullptr);
std::string cached_image_path(const std::string& url);

} // namespace gameslauncher::artwork
