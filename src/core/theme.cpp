#include "core/theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gameslauncher::theme {
namespace {

double channel(int value) {
  const double normalized = static_cast<double>(value) / 255.0;
  return normalized <= 0.04045 ? normalized / 12.92
                               : std::pow((normalized + 0.055) / 1.055, 2.4);
}

} // namespace

bool is_dark_hex(const std::string& hex) {
  if (hex.size() != 7 || hex.front() != '#') {
    return true;
  }
  int values[3] = {0, 0, 0};
  for (int index = 0; index < 3; ++index) {
    const std::string part = hex.substr(1 + index * 2, 2);
    char* end = nullptr;
    const long value = std::strtol(part.c_str(), &end, 16);
    if (end == part.c_str() || *end != '\0') {
      return true;
    }
    values[index] = static_cast<int>(value);
  }
  const double luminance =
      0.2126 * channel(values[0]) + 0.7152 * channel(values[1]) + 0.0722 * channel(values[2]);
  return luminance < 0.5;
}

std::string mono_font_css(const std::string& family) {
  std::string safe = family.empty() ? "monospace" : family;
  safe.erase(std::remove_if(safe.begin(), safe.end(), [](char ch) {
               return ch == '"' || ch == '\n' || ch == '\r' || ch == ';' || ch == '{' || ch == '}';
             }),
             safe.end());
  return "font-family: \"" + safe + "\", \"Adwaita Mono\", \"JetBrains Mono\", monospace;";
}

} // namespace gameslauncher::theme
