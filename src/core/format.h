#pragma once

#include <string>

namespace gameslauncher {

[[nodiscard]] std::string format_playtime(int minutes);
[[nodiscard]] std::string format_last_played(long long unix_seconds);

} // namespace gameslauncher
