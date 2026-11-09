#include "core/launch.h"

#include <glib.h>
#include <giomm/subprocesslauncher.h>
#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace gameslauncher {
namespace {

std::string shell_quote(const std::string& value) {
  std::string result = "'";
  for (char ch : value) {
    if (ch == '\'') {
      result += "'\\''";
    } else {
      result.push_back(ch);
    }
  }
  result.push_back('\'');
  return result;
}

bool env_name(const std::string& value) {
  if (value.empty() || std::isdigit(static_cast<unsigned char>(value.front())) != 0) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_';
  });
}

std::string proton_path_for(const std::string& runner) {
  if (runner.empty() || runner == "Default") {
    return {};
  }
  std::error_code error;
  if (std::filesystem::is_directory(runner, error)) {
    return runner;
  }
  for (const std::string& base :
       {Glib::build_filename(Glib::get_home_dir(), ".local/share/Steam/compatibilitytools.d"),
        Glib::build_filename(Glib::get_home_dir(), ".steam/steam/compatibilitytools.d"),
        Glib::build_filename(Glib::get_home_dir(),
                             ".local/share/faugus-launcher/compatibilitytools")}) {
    const std::string candidate = Glib::build_filename(base, runner);
    if (std::filesystem::is_directory(candidate, error)) {
      return candidate;
    }
  }
  return runner;
}

} // namespace

LaunchCommand steam_launch_command(const Game& game, bool flatpak) {
  LaunchCommand command;
  const std::string& app_id = game.steam_app_id;
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

LaunchCommand store_launch_command(const Game& game) {
  LaunchCommand command;
  if (game.executable.empty() || game.library_path.empty()) {
    return command;
  }
  std::string umu = Glib::build_filename(Glib::get_home_dir(),
                                         ".local/share/faugus-launcher/umu-run");
  if (g_file_test(umu.c_str(), G_FILE_TEST_IS_REGULAR) == 0) {
    umu = Glib::find_program_in_path("umu-run");
  }
  if (umu.empty()) {
    return command;
  }

  std::vector<std::string> parts;
  parts.push_back("WINEPREFIX=" + shell_quote(game.library_path));
  const std::string proton = proton_path_for(game.runner);
  if (!proton.empty()) {
    parts.push_back("PROTONPATH=" + shell_quote(proton));
  }
  std::istringstream stream(game.launch_arguments);
  std::string line;
  while (std::getline(stream, line)) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos || equals == 0) {
      continue;
    }
    const std::string name = line.substr(0, equals);
    const std::string value = line.substr(equals + 1);
    if (!env_name(name)) {
      continue;
    }
    parts.push_back(name + "=" + shell_quote(value));
  }
  parts.push_back(shell_quote(umu));
  parts.push_back(shell_quote(game.executable));

  std::string joined;
  for (const std::string& part : parts) {
    if (!joined.empty()) {
      joined.push_back(' ');
    }
    joined += part;
  }
  if (!Glib::find_program_in_path("faugus-launcher").empty()) {
    command.program = "faugus-launcher";
    command.arguments = {"--run", joined};
  } else if (!Glib::find_program_in_path("flatpak").empty()) {
    command.program = "flatpak";
    command.arguments = {"run", "io.github.Faugus.faugus-launcher", "--run", joined};
  } else {
    command.program = "faugus-launcher";
    command.arguments = {"--run", joined};
  }
  return command;
}

LaunchCommand faugus_launch_command_for(const std::string& gameid) {
  LaunchCommand command;
  if (gameid.empty()) {
    return command;
  }
  if (!Glib::find_program_in_path("faugus-launcher").empty()) {
    command.program = "faugus-launcher";
    command.arguments = {"--game", gameid};
  } else if (!Glib::find_program_in_path("flatpak").empty()) {
    command.program = "flatpak";
    command.arguments = {"run", "io.github.Faugus.faugus-launcher", "--game", gameid};
  } else {
    command.program = "faugus-launcher";
    command.arguments = {"--game", gameid};
  }
  return command;
}

LaunchCommand faugus_launch_command(const Game& game) {
  return faugus_launch_command_for(game.faugus_gameid);
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
      if (command.program == "flatpak") {
        *error = "Flatpak is not installed. Install Steam or Flatpak first.";
      } else if (command.program == "faugus-launcher") {
        *error = "Faugus Launcher is not installed or is not on PATH.";
      } else {
        *error = "Steam is not installed or is not on PATH.";
      }
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
