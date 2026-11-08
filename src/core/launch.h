#pragma once

#include "core/library.h"

#include <string>
#include <vector>

namespace gameslauncher {

struct LaunchCommand {
  std::string program;
  std::vector<std::string> arguments;
};

// Builds an argv-only Steam launch command. The app id must be numeric; no
// title, path or other user text is interpolated into a command.
[[nodiscard]] LaunchCommand steam_launch_command(const Game& game, bool flatpak);

// Starts the command without a shell. Returns false with a bounded message on
// failure. The child is not waited for.
bool launch(const LaunchCommand& command, std::string* error);

} // namespace gameslauncher
