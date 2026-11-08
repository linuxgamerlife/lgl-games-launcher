#pragma once

#include <string>

namespace gameslauncher::theme {

// True when the #rrggbb colour is dark by relative luminance. Used for the
// .dark class that selects the fixed status colours.
[[nodiscard]] bool is_dark_hex(const std::string& hex);

// Produces the mono font rule. The ThemeWatcher replaces the family with the
// desktop monospace font.
[[nodiscard]] std::string mono_font_css(const std::string& family);

} // namespace gameslauncher::theme
