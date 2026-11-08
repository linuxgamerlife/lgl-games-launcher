#include "core/launch.h"

#include <giomm/subprocesslauncher.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>

namespace gameslauncher {

LaunchCommand steam_launch_command(const Game& game, bool flatpak) {
  LaunchCommand command;
  const std::string& app_id = game.steam.app_id;
  if (app_id.empty() || !std::all_of(app_id.begin(), app_id.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
      })) {
    return command;
  }
  if (flatpak) {
    command.program = "flatpak";
    command.arguments = {"run", "com.valvesoftware.Steam", "-applaunch", app_id};
  } else {
    command.program = "steam";
    command.arguments = {"-applaunch", app_id};
  }
  return command;
}

bool launch(const LaunchCommand& command, std::string* error) {
  if (command.program.empty() ||
      !std::all_of(command.arguments.begin(), command.arguments.end(),
                   [](const std::string& value) { return value.find('\0') == std::string::npos; })) {
    if (error != nullptr) {
      *error = "The launch command is empty or malformed.";
    }
    return false;
  }
  if (Glib::find_program_in_path(command.program).empty()) {
    if (error != nullptr) {
      *error = command.program == "flatpak"
                   ? "Flatpak is not installed. Install Steam or Flatpak first."
                   : "Steam is not installed or is not on PATH.";
    }
    return false;
  }
  std::vector<std::string> argv;
  argv.reserve(command.arguments.size() + 1);
  argv.push_back(command.program);
  argv.insert(argv.end(), command.arguments.begin(), command.arguments.end());
  try {
    auto launcher = Gio::SubprocessLauncher::create(Gio::Subprocess::Flags::NONE);
    launcher->set_cwd(Glib::get_home_dir());
    launcher->spawn(argv);
    return true;
  } catch (const Glib::Error& failure) {
    if (error != nullptr) {
      *error = failure.what();
    }
    return false;
  }
}

} // namespace gameslauncher
