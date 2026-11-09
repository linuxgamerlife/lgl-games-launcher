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

// Builds an argv-only Faugus launch command for a Faugus-owned entry. The
// game id is a local slug; no shell is involved.
[[nodiscard]] LaunchCommand faugus_launch_command(const Game& game);

// Launches a store game directly through Faugus's Proton runner. Returns an
// empty program when no executable is known; callers can open the store then.
[[nodiscard]] LaunchCommand store_launch_command(const Game& game);

// Opens a specific Faugus entry, such as a store launcher.
[[nodiscard]] LaunchCommand faugus_launch_command_for(const std::string& gameid);

// Starts the command without a shell. Returns false with a bounded message on
// failure. The child is not waited for.
bool launch(const LaunchCommand& command, std::string* error);

} // namespace gameslauncher
