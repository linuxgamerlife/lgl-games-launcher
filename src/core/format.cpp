#include "core/format.h"

#include <ctime>

namespace gameslauncher {

std::string format_playtime(int minutes) {
  if (minutes <= 0) {
    return "Not played";
  }
  const int hours = minutes / 60;
  const int remainder = minutes % 60;
  if (hours == 0) {
    return std::to_string(remainder) + " min";
  }
  if (remainder == 0) {
    return std::to_string(hours) + " h";
  }
  return std::to_string(hours) + " h " + std::to_string(remainder) + " min";
}

std::string format_last_played(long long unix_seconds) {
  if (unix_seconds <= 0) {
    return "Never";
  }
  const std::time_t value = static_cast<std::time_t>(unix_seconds);
  std::tm local{};
  if (localtime_r(&value, &local) == nullptr) {
    return "Unknown";
  }
  char buffer[64]{};
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local) == 0) {
    return "Unknown";
  }
  return buffer;
}

} // namespace gameslauncher
