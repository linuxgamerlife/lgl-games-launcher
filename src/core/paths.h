#pragma once

#include <string>

namespace gameslauncher {

// XDG locations for the application. These never touch the user's real
// directories inside tests; the test harness sets XDG_* to a temporary root.
std::string config_dir();
std::string data_dir();
std::string cache_dir();
std::string settings_path();
std::string database_path();

// Creates a private directory (0700). Returns false and logs on failure.
bool ensure_private_dir(const std::string& path);

} // namespace gameslauncher
