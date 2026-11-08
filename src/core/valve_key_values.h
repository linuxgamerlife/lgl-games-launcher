#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace gameslauncher::vdf {

// A small, bounded Valve KeyValues reader for libraryfolders.vdf and
// appmanifest_*.acf. It is not a general-purpose VDF library: it accepts the
// quoted/bare keys and objects those files use, returns an error instead of
// guessing, and never follows a path or executes anything.
struct Node {
  std::map<std::string, Node> children;
  std::string value;
  bool is_object = false;

  [[nodiscard]] const Node* child(std::string_view key) const;
  [[nodiscard]] std::string string_value(std::string_view key) const;
  [[nodiscard]] long long integer_value(std::string_view key, long long fallback = 0) const;
};

// Parses a complete document. On failure returns nullopt and sets error.
// The caller must bound the input size before calling (the scanner does).
std::optional<Node> parse(std::string_view text, std::string* error = nullptr);

} // namespace gameslauncher::vdf
