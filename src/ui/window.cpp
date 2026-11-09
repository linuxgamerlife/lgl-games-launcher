#include "ui/window.h"

#include "ui/epic_login.h"
#include "ui/ubisoft_login.h"

#include "core/artwork.h"
#include "core/secret_store.h"
#include "core/format.h"
#include "core/launch.h"
#include "core/paths.h"

#include <adwaita.h>
#include <giomm/menu.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/grid.h>
#include <gtkmm/separator.h>

#include <algorithm>
#include <string>

namespace gameslauncher {
namespace {

constexpr const char* kIconResource =
    "/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.png";

} // namespace

Window::Window(const Glib::RefPtr<Gtk::Application>& application)
    : Gtk::ApplicationWindow(application) {
  set_title("LGL Games Launcher");
  set_default_size(m_settings.window_width(), m_settings.window_height());
  set_size_request(640, 420);

  m_theme.install();
  m_theme_connection = m_theme.signal_changed().connect([this] { update_dark_class(); });
  m_artwork_connection = m_artwork_dispatcher.connect([this] {
    apply_cached_covers();
    m_library.set_games(m_games);
    if (!m_details.current_app_id().empty()) {
      if (const Game* game = find_game(m_details.current_app_id())) {
        m_details.update_artwork(*game);
      }
    }
  });
  m_igdb_connection = m_igdb_dispatcher.connect([this] {
    std::string app_id;
    IgdbInfo info;
    {
      std::lock_guard lock(m_metadata_mutex);
      app_id = m_pending_igdb_app;
      info = m_pending_igdb;
    }
    if (app_id == m_details.current_app_id()) {
      m_details.set_igdb(info);
    }
  });
  m_ubisoft_connection = m_ubisoft_dispatcher.connect([this] {
    std::vector<UbisoftGame> games;
    std::optional<UbisoftSession> session;
    std::string error;
    {
      std::lock_guard lock(m_ubisoft_result_mutex);
      games = std::move(m_pending_ubisoft_games);
      session = std::move(m_pending_ubisoft_session);
      error = std::move(m_pending_ubisoft_error);
      m_pending_ubisoft_games.clear();
      m_pending_ubisoft_session.reset();
      m_pending_ubisoft_error.clear();
    }
    ubisoft_log("window: dispatch games=" + std::to_string(games.size()) +
                " error=" + error);
    m_ubisoft_last_error = error;
    m_ubisoft_last_count = games.size();
    if (!error.empty()) {
      update_ubisoft_status();
      show_notice("Ubisoft: " + error, true);
      return;
    }
    if (session.has_value()) {
      m_ubisoft_session = session;
      (void)UbisoftClient::save_session(*session);
    }
    m_ubisoft_games = std::move(games);
    UbisoftClient::save_games_cache(m_ubisoft_games);
    merge_ubisoft_games();
    update_ubisoft_status();
    if (m_ubisoft_games.empty()) {
      show_notice("Ubisoft returned no owned games.", true);
    } else {
      show_notice("Ubisoft library loaded: " + std::to_string(m_ubisoft_games.size()) +
                  " games.");
    }
  });
  m_epic_connection = m_epic_dispatcher.connect([this] {
    std::vector<EpicGame> games;
    std::optional<EpicSession> session;
    std::string error;
    {
      std::lock_guard lock(m_epic_result_mutex);
      games = std::move(m_pending_epic_games);
      session = std::move(m_pending_epic_session);
      error = std::move(m_pending_epic_error);
      m_pending_epic_games.clear();
      m_pending_epic_session.reset();
      m_pending_epic_error.clear();
    }
    epic_log("window: dispatch games=" + std::to_string(games.size()) +
             " error=" + error);
    m_epic_last_error = error;
    m_epic_last_count = games.size();
    if (!error.empty()) {
      update_epic_status();
      show_notice("Epic: " + error, true);
      return;
    }
    if (session.has_value()) {
      m_epic_session = session;
      (void)EpicClient::save_session(*session);
    }
    m_epic_games = std::move(games);
    EpicClient::save_games_cache(m_epic_games);
    merge_epic_games();
    update_epic_status();
    if (m_epic_games.empty()) {
      show_notice("Epic returned no owned games. Check that this account owns PC games.", true);
    } else {
      show_notice("Epic library loaded: " + std::to_string(m_epic_games.size()) + " games.");
    }
  });
  m_achievements_connection = m_achievements_dispatcher.connect([this] {
    std::string app_id;
    SteamAchievementsResult result;
    {
      std::lock_guard lock(m_metadata_mutex);
      app_id = m_pending_achievements_app;
      result = m_pending_achievements;
    }
    if (app_id == m_details.current_app_id()) {
      m_details.set_achievements(result);
    }
  });

  build_header();

  m_stack.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
  m_stack.add(m_library, "library");
  m_stack.add(m_details, "details");
  m_stack.set_visible_child(m_library);

  m_content.set_orientation(Gtk::Orientation::VERTICAL);
  m_content.append(m_stack);
  m_overlay.set_child(m_content);

  m_notice.add_css_class("gl-notice");
  m_notice.set_wrap(true);
  m_notice.set_max_width_chars(80);
  m_notice.set_halign(Gtk::Align::CENTER);
  m_notice_revealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_UP);
  m_notice_revealer.set_halign(Gtk::Align::CENTER);
  m_notice_revealer.set_valign(Gtk::Align::END);
  m_notice_revealer.set_margin_bottom(16);
  m_notice_revealer.set_can_target(false);
  m_notice_revealer.set_child(m_notice);
  m_overlay.add_overlay(m_notice_revealer);
  set_child(m_overlay);

  auto back_click = Gtk::GestureClick::create();
  back_click->set_button(8);
  back_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  back_click->signal_pressed().connect([this](int, double, double) { go_back(); });
  add_controller(back_click);

  auto forward_click = Gtk::GestureClick::create();
  forward_click->set_button(9);
  forward_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  forward_click->signal_pressed().connect([this](int, double, double) { go_forward(); });
  add_controller(forward_click);

  m_library.signal_open_game().connect([this](const std::string& app_id) {
    open_game(app_id);
  });
  m_library.signal_sort_changed().connect([this](const std::string& sort) {
    m_settings.set_sort_mode(sort);
  });
  m_library.signal_show_hidden_changed().connect([this](bool show) {
    m_settings.set_show_hidden(show);
  });
  m_library.signal_installed_only_changed().connect([this](bool installed) {
    m_settings.set_installed_only(installed);
  });
  m_details.signal_back().connect([this] { go_back(); });
  m_details.signal_play().connect([this](const std::string& app_id) { play(app_id); });
  m_details.signal_favorite().connect(
      [this](const std::string& app_id, bool value) { toggle_favorite(app_id, value); });
  m_details.signal_hide_game().connect(
      [this](const std::string& app_id, bool value) { toggle_hidden(app_id, value); });
  m_details.signal_manage().connect(
      [this](const std::string& app_id) { open_in_steam(app_id); });
  m_details.signal_tags().connect(
      [this](const std::string& app_id, const std::string& tags) { set_tags(app_id, tags); });
  m_details.signal_status().connect(
      [this](const std::string& app_id, const std::string& status) {
        set_status(app_id, status);
      });
  m_details.signal_refresh_igdb().connect(
      [this](const std::string& app_id) { load_igdb(app_id); });
  m_details.signal_refresh_achievements().connect(
      [this](const std::string& app_id) { load_achievements(app_id); });
  m_details.signal_connect_igdb().connect([this] { show_connections(); });

  std::string database_error;
  if (!m_flags.open(database_path(), &database_error)) {
    show_notice("Could not open the personal library database: " + database_error, true, true);
  }

  apply_title_bar(m_settings.system_title_bar(), false);
  m_library.set_sort_mode(m_settings.sort_mode());
  m_library.set_show_hidden(m_settings.show_hidden());
  m_library.set_installed_only(m_settings.installed_only());

  auto* manager = adw_style_manager_get_default();
  if (manager != nullptr) {
    m_dark_handler = g_signal_connect(manager, "notify::dark",
                                      G_CALLBACK(on_style_manager_dark), this);
    update_dark_class();
  }

  signal_close_request().connect(
      [this] {
        m_settings.set_window_width(get_width());
        m_settings.set_window_height(get_height());
        return false;
      },
      false);

  start_metadata_workers();
  start_epic_worker();
  m_epic_session = EpicClient::load_session();
  if (m_epic_session.has_value()) {
    m_epic_games = EpicClient::load_games_cache();
    request_epic_library();
  }
  start_ubisoft_worker();
  m_ubisoft_session = UbisoftClient::load_session();
  if (m_ubisoft_session.has_value()) {
    m_ubisoft_games = UbisoftClient::load_games_cache();
    request_ubisoft_library();
  }
  m_scan_idle = Glib::signal_idle().connect([this] {
    scan_and_load();
    return false;
  });
}

Window::~Window() {
  m_scan_idle.disconnect();
  m_notice_timeout.disconnect();
  if (m_igdb_thread.joinable()) {
    m_igdb_thread.request_stop();
    m_igdb_cv.notify_all();
    m_igdb_thread.join();
  }
  if (m_achievements_thread.joinable()) {
    m_achievements_thread.request_stop();
    m_achievements_cv.notify_all();
    m_achievements_thread.join();
  }
  if (m_epic_thread.joinable()) {
    m_epic_thread.request_stop();
    m_epic_cv.notify_all();
    m_epic_thread.join();
  }
  if (m_ubisoft_thread.joinable()) {
    m_ubisoft_thread.request_stop();
    m_ubisoft_cv.notify_all();
    m_ubisoft_thread.join();
  }
  for (std::jthread& thread : m_artwork_threads) {
    if (thread.joinable()) {
      thread.request_stop();
      thread.join();
    }
  }
  m_theme_connection.disconnect();
  m_artwork_connection.disconnect();
  m_igdb_connection.disconnect();
  m_achievements_connection.disconnect();
  m_epic_connection.disconnect();
  m_ubisoft_connection.disconnect();
  if (m_dark_handler != 0) {
    auto* manager = adw_style_manager_get_default();
    if (manager != nullptr) {
      g_signal_handler_disconnect(manager, m_dark_handler);
    }
  }
}

void Window::build_header() {
  m_logo.set_orientation(Gtk::Orientation::HORIZONTAL);
  m_logo.set_spacing(10);
  m_logo.set_margin_end(12);
  m_icon.set_from_resource(kIconResource);
  m_icon.set_pixel_size(28);
  m_icon.add_css_class("brand-icon");
  m_logo.append(m_icon);
  m_wordmark.set_text("LGL GAMES LAUNCHER");
  m_wordmark.add_css_class("wordmark");
  m_logo.append(m_wordmark);
  m_header.pack_start(m_logo);

  auto* empty_title = Gtk::make_managed<Gtk::Box>();
  m_header.set_title_widget(*empty_title);

  m_home_button.set_icon_name("go-home-symbolic");
  m_home_button.set_tooltip_text("Show all games (clear filters)");
  m_home_button.set_action_name("win.home");
  m_header.pack_end(m_home_button);

  m_refresh_button.set_icon_name("view-refresh-symbolic");
  m_refresh_button.set_tooltip_text("Rescan library (Ctrl+R)");
  m_refresh_button.set_action_name("win.refresh");
  m_header.pack_end(m_refresh_button);

  auto menu = Gio::Menu::create();

  auto library_section = Gio::Menu::create();
  library_section->append("Show All Games", "win.home");
  library_section->append("Refresh library", "win.refresh");
  library_section->append("Search", "win.search");
  menu->append_section(library_section);

  auto window_section = Gio::Menu::create();
  window_section->append("Use System Title Bar", "win.system-title-bar");
  window_section->append("Connections…", "win.connections");
  menu->append_section(window_section);

  auto help_section = Gio::Menu::create();
  help_section->append("About LGL Games Launcher", "win.about");
  help_section->append("Quit", "app.quit");
  menu->append_section(help_section);

  m_menu_button.set_icon_name("open-menu-symbolic");
  m_menu_button.set_tooltip_text("Main menu");
  m_menu_button.set_primary(true);
  m_menu_button.set_menu_model(menu);
  m_header.pack_end(m_menu_button);

  add_action("refresh", sigc::mem_fun(*this, &Window::refresh));
  add_action("home", [this] {
    m_stack.set_visible_child(m_library);
    m_library.show_all();
  });
  add_action("search", [this] { m_library.focus_search(); });
  add_action("about", [this] { show_about(); });
  add_action("connections", [this] { show_connections(); });
  add_action("back", [this] { go_back(); });
  add_action("forward", [this] { go_forward(); });

  m_title_action =
      Gio::SimpleAction::create_bool("system-title-bar", m_settings.system_title_bar());
  m_title_action->signal_activate().connect([this](const Glib::VariantBase&) {
    const bool next = !m_system_title_bar;
    m_title_action->set_state(Glib::Variant<bool>::create(next));
    apply_title_bar(next, true);
  });
  add_action(m_title_action);

  if (auto application = get_application()) {
    application->set_accel_for_action("win.refresh", "<Control>r");
    application->set_accel_for_action("win.search", "<Control>f");
    application->set_accel_for_action("win.system-title-bar", "<Control><Shift>m");
    application->set_accel_for_action("win.home", "<Control>h");
    application->set_accel_for_action("win.back", "<Alt>Left");
    application->set_accel_for_action("win.forward", "<Alt>Right");
  }
}

void Window::apply_title_bar(bool system, bool runtime) {
  if (!runtime && system == m_system_title_bar && get_titlebar() != nullptr) {
    return;
  }
  auto application = get_application();
  if (runtime) {
    if (application) {
      application->hold();
    }
    set_visible(false);
    Gtk::Widget::unrealize();
  }

  if (system) {
    m_header.set_show_title_buttons(false);
    unset_titlebar();
    if (m_header.get_parent() != &m_content) {
      m_content.prepend(m_header);
    }
  } else {
    if (m_header.get_parent() == &m_content) {
      m_content.remove(m_header);
    }
    m_header.set_show_title_buttons(true);
    set_titlebar(m_header);
  }
  m_system_title_bar = system;

  if (runtime) {
    if (application) {
      set_application(application);
      present();
      application->release();
    } else {
      present();
    }
  }
}

void Window::scan_and_load() {
  m_scan = SteamScanner::scan();
  m_faugus_scan = FaugusScanner::scan();
  m_store_scan = StoreScanner::scan(m_faugus_scan);
  m_games = load_games(m_scan, m_faugus_scan, m_store_scan, m_flags);
  apply_cached_covers();
  m_library.set_games(m_games);
  std::vector<std::string> warnings = m_scan.warnings;
  warnings.insert(warnings.end(), m_faugus_scan.warnings.begin(),
                  m_faugus_scan.warnings.end());
  warnings.insert(warnings.end(), m_store_scan.warnings.begin(),
                  m_store_scan.warnings.end());
  if (m_games.empty()) {
    show_notice(warnings.empty()
                    ? "No games found. Check that Steam or Faugus is installed and has a library."
                    : warnings.front(),
                true);
  } else if (!warnings.empty()) {
    show_notice(warnings.front());
  }
  fetch_missing_covers();
  if (!m_epic_games.empty() || !m_ubisoft_games.empty()) {
    rebuild_games();
  }
}

void Window::apply_cached_covers() {
  for (Game& game : m_games) {
    if (game.cover_path.empty()) {
      if (game.is_steam()) {
        game.cover_path = artwork::cached_cover_path(game.steam_app_id);
      } else if (game.kind == GameKind::Store) {
        game.cover_path = artwork::cached_store_cover(game.title);
      }
    }
    if (game.kind == GameKind::Store && game.hero_path.empty()) {
      game.hero_path = artwork::cached_store_hero(game.title);
    }
  }
}

void Window::fetch_missing_covers() {
  struct Task {
    std::string steam_app_id;
    std::string title;
    std::string source;
    bool store = false;
  };
  std::vector<Task> missing;
  for (const Game& game : m_games) {
    if (!game.cover_path.empty()) {
      continue;
    }
    if (game.is_steam()) {
      missing.push_back({game.steam_app_id, {}, {}, false});
    } else if (game.kind == GameKind::Store) {
      missing.push_back({{}, game.title, game.source, true});
    }
  }
  m_artwork_threads.clear();
  const std::size_t workers = std::min<std::size_t>(4, missing.size());
  for (std::size_t worker = 0; worker < workers; ++worker) {
    m_artwork_threads.emplace_back(
        [this, missing, worker, workers](std::stop_token stop) {
          for (std::size_t index = worker; index < missing.size(); index += workers) {
            if (stop.stop_requested()) {
              return;
            }
            if (missing[index].store) {
              artwork::ensure_store_artwork(missing[index].title, nullptr);
            } else {
              artwork::fetch_steam_cover(missing[index].steam_app_id, nullptr, nullptr);
            }
          }
          m_artwork_dispatcher.emit();
        });
  }
}

void Window::refresh() {
  scan_and_load();
  if (m_scan.warnings.empty() && m_faugus_scan.warnings.empty() &&
      m_store_scan.warnings.empty() && !m_games.empty()) {
    show_notice("Library refreshed: " + std::to_string(m_games.size()) + " games.");
  }
}

void Window::open_game(const std::string& app_id) {
  const Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  if (m_history_pos + 1 < m_history.size()) {
    m_history.resize(m_history_pos + 1);
  }
  if (m_history.empty() || m_history.back() != app_id) {
    if (m_history.size() > 500) {
      m_history.erase(m_history.begin() + 1);
      m_history_pos = m_history.size() - 1;
    }
    m_history.push_back(app_id);
    m_history_pos = m_history.size() - 1;
  }
  show_game(app_id);
}

void Window::show_game(const std::string& app_id) {
  const Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  m_details.set_game(*game);
  m_stack.set_visible_child(m_details);
  load_igdb(app_id);
  load_achievements(app_id);
}

void Window::navigate_history() {
  if (m_history.empty()) {
    m_stack.set_visible_child(m_library);
    return;
  }
  if (m_history_pos == 0) {
    m_stack.set_visible_child(m_library);
    return;
  }
  show_game(m_history[m_history_pos]);
}

void Window::go_back() {
  if (m_history_pos > 0) {
    --m_history_pos;
  }
  navigate_history();
}

void Window::go_forward() {
  if (m_history_pos + 1 < m_history.size()) {
    ++m_history_pos;
  }
  navigate_history();
}

void Window::play(const std::string& app_id) {
  const Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  if (game->kind == GameKind::Store) {
    Game launch_game = *game;
    if (!game->installed && !game->install_uri.empty()) {
      launch_game.launch_uri = game->install_uri;
    }
    LaunchCommand direct = store_launch_command(launch_game);
    epic_log("play: " + game->title + " installed=" +
             (game->installed ? "true" : "false") + " uri=" + launch_game.launch_uri +
             " exe=" + launch_game.executable);
    epic_log("play: program=" + direct.program);
    for (const std::string& argument : direct.arguments) {
      epic_log("play: arg=" + argument);
    }
    std::string error;
    if (direct.program.empty()) {
      const LaunchCommand client = faugus_launch_command_for(game->store_launcher_gameid);
      if (!launch(client, &error)) {
        show_notice(error, true);
        return;
      }
      show_notice("Opening " + game->source + ". Start " + game->title +
                  " from the client.");
      return;
    }
    if (!launch(direct, &error)) {
      show_notice(error, true);
      return;
    }
    if (!game->installed && !game->install_uri.empty()) {
      show_notice("Opening " + game->source + " to install " + game->title + "…");
    } else {
      show_notice("Launching " + game->title + "…");
    }
    return;
  }

  const LaunchCommand command = game->is_steam()
                                    ? steam_launch_command(*game, game->flatpak)
                                    : faugus_launch_command(*game);
  std::string error;
  if (!launch(command, &error)) {
    show_notice(error, true);
    return;
  }
  show_notice("Launching " + game->title + "…");
}

void Window::toggle_favorite(const std::string& app_id, bool favorite) {
  Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  GameFlags flags = game->flags;
  flags.favorite = favorite;
  update_game_flags(app_id, flags);
  show_notice(favorite ? "Added to favourites." : "Removed from favourites.");
}

void Window::toggle_hidden(const std::string& app_id, bool hidden) {
  Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  GameFlags flags = game->flags;
  flags.hidden = hidden;
  update_game_flags(app_id, flags);
  show_notice(hidden ? "Game hidden." : "Game unhidden.");
  if (hidden && !m_settings.show_hidden()) {
    go_back();
  }
}

void Window::set_tags(const std::string& app_id, const std::string& tags) {
  if (tags.size() > 4096) {
    show_notice("Tags are too long.", true);
    return;
  }
  Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  GameFlags flags = game->flags;
  flags.tags = tags;
  update_game_flags(app_id, flags);
  show_notice("Tags saved.");
}

void Window::update_game_flags(const std::string& app_id, const GameFlags& flags) {
  Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  if (!m_flags.set(app_id, flags)) {
    show_notice("Could not save the game flags.", true);
    return;
  }
  game->flags = flags;
  m_library.update_game_flags(app_id, flags);
  if (m_details.current_app_id() == app_id) {
    m_details.update_flags(flags);
  }
}

void Window::show_notice(const std::string& text, bool error, bool sticky) {
  m_notice.set_text(text);
  if (error) {
    m_notice.add_css_class("negative");
  } else {
    m_notice.remove_css_class("negative");
  }
  m_notice_revealer.set_reveal_child(true);
  m_notice_timeout.disconnect();
  if (!sticky) {
    m_notice_timeout = Glib::signal_timeout().connect(
        [this] {
          m_notice_revealer.set_reveal_child(false);
          return false;
        },
        5000);
  }
}

void Window::set_status(const std::string& app_id, const std::string& status) {
  Game* game = find_game(app_id);
  if (game == nullptr) {
    return;
  }
  GameFlags flags = game->flags;
  flags.status = status;
  update_game_flags(app_id, flags);
}

void Window::load_igdb(const std::string& app_id) {
  if (app_id.empty()) {
    return;
  }
  const std::string client_id = m_settings.igdb_client_id();
  const std::string secret = SecretStore::lookup("igdb-client-secret");
  const Game* game = find_game(app_id);
  const std::string title = game != nullptr ? game->title : std::string();
  const std::string steam_app_id = game != nullptr ? game->steam_app_id : std::string();
  if (client_id.empty() || secret.empty()) {
    IgdbInfo info;
    info.error = "Connect IGDB in Settings to see critic scores and length estimates.";
    if (app_id == m_details.current_app_id()) {
      m_details.set_igdb(info);
    }
    return;
  }
  if (app_id == m_details.current_app_id()) {
    m_details.set_igdb_loading();
  }
  {
    std::lock_guard lock(m_igdb_mutex);
    m_igdb_request.app_id = app_id;
    m_igdb_request.steam_app_id = steam_app_id;
    m_igdb_request.title = title;
    m_igdb_request.client_id = client_id;
    m_igdb_request.client_secret = secret;
    m_igdb_request.pending = true;
  }
  m_igdb_cv.notify_one();
}

void Window::load_achievements(const std::string& app_id) {
  if (app_id.empty()) {
    return;
  }
  const Game* current = find_game(app_id);
  if (current != nullptr && !current->is_steam()) {
    return;
  }
  if (m_scan.steam_id.empty()) {
    SteamAchievementsResult result;
    result.error = "No Steam profile was found. Sign in to Steam once, then refresh.";
    if (app_id == m_details.current_app_id()) {
      m_details.set_achievements(result);
    }
    return;
  }
  if (app_id == m_details.current_app_id()) {
    m_details.set_achievements_loading();
  }
  const std::string steam_id = m_scan.steam_id;
  {
    std::lock_guard lock(m_achievements_mutex);
    m_achievements_request.app_id = app_id;
    m_achievements_request.steam_id = steam_id;
    m_achievements_request.pending = true;
  }
  m_achievements_cv.notify_one();
}

void Window::show_connections() {
  if (!m_connections) {
    m_connections = std::make_unique<Gtk::Dialog>();
    m_connections->set_title("Connections");
    m_connections->set_icon_name("lgl-games-launcher");
    m_connections->set_transient_for(*this);
    m_connections->set_modal(true);
    m_connections->set_default_size(480, -1);
    m_connections->set_hide_on_close(true);
    auto* content = m_connections->get_content_area();
    content->set_margin(18);
    content->set_spacing(12);

    auto* note = Gtk::make_managed<Gtk::Label>(
        "IGDB uses your Twitch developer client ID and secret. The secret is stored "
        "in the desktop Secret Service, never in settings.ini.");
    note->set_wrap(true);
    note->set_xalign(0.0F);
    note->add_css_class("muted");
    content->append(*note);

    auto* grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(8);
    grid->set_column_spacing(12);
    auto* id_label = Gtk::make_managed<Gtk::Label>("IGDB client ID");
    id_label->set_xalign(0.0F);
    grid->attach(*id_label, 0, 0, 1, 1);
    m_igdb_client_id_entry = Gtk::make_managed<Gtk::Entry>();
    m_igdb_client_id_entry->set_hexpand(true);
    m_igdb_client_id_entry->set_text(m_settings.igdb_client_id());
    grid->attach(*m_igdb_client_id_entry, 1, 0, 1, 1);
    auto* secret_label = Gtk::make_managed<Gtk::Label>("IGDB client secret");
    secret_label->set_xalign(0.0F);
    grid->attach(*secret_label, 0, 1, 1, 1);
    m_igdb_secret_entry = Gtk::make_managed<Gtk::Entry>();
    m_igdb_secret_entry->set_hexpand(true);
    m_igdb_secret_entry->set_visibility(false);
    m_igdb_secret_entry->set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_igdb_secret_entry->set_placeholder_text(
        SecretStore::lookup("igdb-client-secret").empty()
            ? "Twitch client secret"
            : "Stored secret — leave blank to keep");
    grid->attach(*m_igdb_secret_entry, 1, 1, 1, 1);
    content->append(*grid);

    auto* epic_separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
    content->append(*epic_separator);
    auto* epic_label = Gtk::make_managed<Gtk::Label>("EPIC GAMES");
    epic_label->add_css_class("section-label");
    epic_label->set_xalign(0.0F);
    content->append(*epic_label);
    auto* epic_note = Gtk::make_managed<Gtk::Label>(
        "Sign in through Epic's own login page. The session token is stored in the "
        "desktop Secret Service; your Epic password never reaches LGL.");
    epic_note->set_wrap(true);
    epic_note->set_xalign(0.0F);
    epic_note->add_css_class("muted");
    epic_note->add_css_class("small");
    content->append(*epic_note);
    m_epic_status = Gtk::make_managed<Gtk::Label>();
    m_epic_status->set_xalign(0.0F);
    m_epic_status->add_css_class("muted");
    content->append(*m_epic_status);
    auto* epic_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto* epic_sign_in = Gtk::make_managed<Gtk::Button>("SIGN IN TO EPIC");
    epic_sign_in->add_css_class("gl-small");
    epic_sign_in->signal_clicked().connect([this] { sign_in_epic(); });
    epic_buttons->append(*epic_sign_in);
    auto* epic_refresh = Gtk::make_managed<Gtk::Button>("REFRESH EPIC LIBRARY");
    epic_refresh->add_css_class("gl-small");
    epic_refresh->signal_clicked().connect([this] {
      request_epic_library();
      show_notice("Refreshing the Epic library…");
    });
    epic_buttons->append(*epic_refresh);
    auto* epic_sign_out = Gtk::make_managed<Gtk::Button>("SIGN OUT");
    epic_sign_out->add_css_class("gl-small");
    epic_sign_out->signal_clicked().connect([this] {
      sign_out_epic();
      update_epic_status();
    });
    epic_buttons->append(*epic_sign_out);
    content->append(*epic_buttons);

    auto* ubisoft_separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
    content->append(*ubisoft_separator);
    auto* ubisoft_label = Gtk::make_managed<Gtk::Label>("UBISOFT CONNECT");
    ubisoft_label->add_css_class("section-label");
    ubisoft_label->set_xalign(0.0F);
    content->append(*ubisoft_label);
    auto* ubisoft_note = Gtk::make_managed<Gtk::Label>(
        "Sign in through Ubisoft's own login page. The session is stored in the desktop "
        "Secret Service; your Ubisoft password never reaches LGL.");
    ubisoft_note->set_wrap(true);
    ubisoft_note->set_xalign(0.0F);
    ubisoft_note->add_css_class("muted");
    ubisoft_note->add_css_class("small");
    content->append(*ubisoft_note);
    m_ubisoft_status = Gtk::make_managed<Gtk::Label>();
    m_ubisoft_status->set_xalign(0.0F);
    m_ubisoft_status->add_css_class("muted");
    content->append(*m_ubisoft_status);
    auto* ubisoft_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto* ubisoft_sign_in = Gtk::make_managed<Gtk::Button>("SIGN IN TO UBISOFT");
    ubisoft_sign_in->add_css_class("gl-small");
    ubisoft_sign_in->signal_clicked().connect([this] { sign_in_ubisoft(); });
    ubisoft_buttons->append(*ubisoft_sign_in);
    auto* ubisoft_refresh = Gtk::make_managed<Gtk::Button>("REFRESH UBISOFT LIBRARY");
    ubisoft_refresh->add_css_class("gl-small");
    ubisoft_refresh->signal_clicked().connect([this] {
      request_ubisoft_library();
      show_notice("Refreshing the Ubisoft library…");
    });
    ubisoft_buttons->append(*ubisoft_refresh);
    auto* ubisoft_sign_out = Gtk::make_managed<Gtk::Button>("SIGN OUT");
    ubisoft_sign_out->add_css_class("gl-small");
    ubisoft_sign_out->signal_clicked().connect([this] {
      sign_out_ubisoft();
      update_ubisoft_status();
    });
    ubisoft_buttons->append(*ubisoft_sign_out);
    content->append(*ubisoft_buttons);

    m_connections->add_button("Remove", 2);
    m_connections->add_button("Cancel", static_cast<int>(Gtk::ResponseType::CANCEL));
    m_connections->add_button("Save", static_cast<int>(Gtk::ResponseType::OK));
    m_connections->set_default_response(static_cast<int>(Gtk::ResponseType::OK));
    m_connections->signal_response().connect([this](int response) {
      if (response == static_cast<int>(Gtk::ResponseType::OK) && m_igdb_client_id_entry != nullptr &&
          m_igdb_secret_entry != nullptr) {
        m_settings.set_igdb_client_id(m_igdb_client_id_entry->get_text());
        const std::string secret = m_igdb_secret_entry->get_text();
        if (!secret.empty()) {
          std::string error;
          if (!SecretStore::store("igdb-client-secret", secret, &error)) {
            show_notice("Could not store the IGDB secret: " + error, true);
            return;
          }
        }
        show_notice("IGDB connection saved.");
        if (!m_details.current_app_id().empty()) {
          load_igdb(m_details.current_app_id());
        }
      } else if (response == 2) {
        m_settings.set_igdb_client_id({});
        SecretStore::remove("igdb-client-secret");
        if (m_igdb_client_id_entry != nullptr) {
          m_igdb_client_id_entry->set_text({});
        }
        if (m_igdb_secret_entry != nullptr) {
          m_igdb_secret_entry->set_text({});
        }
        show_notice("IGDB connection removed.");
      }
      if (m_connections) {
        m_connections->set_visible(false);
      }
    });
  }
  if (m_igdb_client_id_entry != nullptr) {
    m_igdb_client_id_entry->set_text(m_settings.igdb_client_id());
  }
  if (m_igdb_secret_entry != nullptr) {
    m_igdb_secret_entry->set_text({});
    m_igdb_secret_entry->set_placeholder_text(
        SecretStore::lookup("igdb-client-secret").empty()
            ? "Twitch client secret"
            : "Stored secret — leave blank to keep");
  }
  update_epic_status();
  update_ubisoft_status();
  m_connections->present();
}

void Window::open_in_steam(const std::string& app_id) {
  const Game* game = find_game(app_id);
  if (game == nullptr || app_id.empty()) {
    return;
  }
  LaunchCommand command;
  if (!game->is_steam()) {
    if (game->kind == GameKind::Store && !game->store_uri.empty()) {
      Game store = *game;
      store.launch_uri = game->store_uri;
      command = store_launch_command(store);
    } else if (game->kind == GameKind::Store) {
      command = faugus_launch_command_for(game->store_launcher_gameid);
    } else if (!Glib::find_program_in_path("faugus-launcher").empty()) {
      command.program = "faugus-launcher";
    } else {
      command.program = "flatpak";
      command.arguments = {"run", "io.github.Faugus.faugus-launcher"};
    }
    std::string error;
    if (!launch(command, &error)) {
      show_notice("Could not open " + game->source + ": " + error, true);
    }
    return;
  }
  LaunchCommand steam_command;
  const std::string uri = "steam://nav/games/details/" + app_id;
  if (game->flatpak) {
    steam_command.program = "flatpak";
    steam_command.arguments = {"run", "com.valvesoftware.Steam", uri};
  } else {
    steam_command.program = "steam";
    steam_command.arguments = {uri};
  }
  std::string error;
  if (!launch(steam_command, &error)) {
    show_notice("Could not open Steam: " + error, true);
  }
}

void Window::start_metadata_workers() {
  m_igdb_thread = std::jthread([this](std::stop_token stop) {
    while (!stop.stop_requested()) {
      IgdbRequest request;
      {
        std::unique_lock lock(m_igdb_mutex);
        if (!m_igdb_cv.wait(lock, stop, [this] { return m_igdb_request.pending; })) {
          return;
        }
        request = m_igdb_request;
        m_igdb_request.pending = false;
      }
      IgdbClient client(request.client_id, request.client_secret);
      IgdbInfo info;
      std::string error;
      if (!client.fetch(request.steam_app_id, request.title, &info, &error)) {
        info.error = error.empty() ? "IGDB data is unavailable." : error;
      }
      {
        std::lock_guard lock(m_metadata_mutex);
        m_pending_igdb_app = request.app_id;
        m_pending_igdb = info;
      }
      m_igdb_dispatcher.emit();
    }
  });

  m_achievements_thread = std::jthread([this](std::stop_token stop) {
    while (!stop.stop_requested()) {
      AchievementRequest request;
      {
        std::unique_lock lock(m_achievements_mutex);
        if (!m_achievements_cv.wait(lock, stop,
                                    [this] { return m_achievements_request.pending; })) {
          return;
        }
        request = m_achievements_request;
        m_achievements_request.pending = false;
      }
      SteamAchievementsResult result =
          fetch_steam_achievements(request.steam_id, request.app_id);
      {
        std::lock_guard lock(m_metadata_mutex);
        m_pending_achievements_app = request.app_id;
        m_pending_achievements = result;
      }
      m_achievements_dispatcher.emit();
    }
  });
}

void Window::start_epic_worker() {
  m_epic_thread = std::jthread([this](std::stop_token stop) {
    while (!stop.stop_requested()) {
      EpicRequest request;
      {
        std::unique_lock lock(m_epic_mutex);
        if (!m_epic_cv.wait(lock, stop, [this] { return m_epic_request.pending; })) {
          return;
        }
        request = m_epic_request;
        m_epic_request.pending = false;
      }
      EpicSession session = request.session;
      std::string error;
      bool ok = EpicClient::refresh(&session, &error);
      std::optional<EpicSession> saved;
      std::vector<EpicGame> games;
      if (ok) {
        (void)EpicClient::save_session(session);
        saved = session;
        ok = EpicClient::fetch_owned_games(session, &games, &error);
      }
      if (ok) {
        for (EpicGame& game : games) {
          if (!game.cover_url.empty()) {
            game.cover_path = artwork::fetch_image(game.cover_url, nullptr);
          }
          if (!game.hero_url.empty()) {
            game.hero_path = artwork::fetch_image(game.hero_url, nullptr);
          }
          if (game.cover_path.empty()) {
            artwork::ensure_store_artwork(game.title, nullptr);
            game.cover_path = artwork::cached_store_cover(game.title);
          }
          if (game.hero_path.empty()) {
            game.hero_path = game.cover_path;
          }
        }
      }
      epic_log("worker: fetched=" + std::to_string(games.size()) +
               " error=" + (ok ? std::string() : error));
      {
        std::lock_guard lock(m_epic_result_mutex);
        m_pending_epic_games = std::move(games);
        m_pending_epic_session = std::move(saved);
        m_pending_epic_error = ok ? std::string() : error;
      }
      m_epic_dispatcher.emit();
    }
  });
}

void Window::request_epic_library() {
  if (!m_epic_session.has_value()) {
    return;
  }
  {
    std::lock_guard lock(m_epic_mutex);
    m_epic_request.session = *m_epic_session;
    m_epic_request.pending = true;
  }
  m_epic_cv.notify_one();
}

void Window::sign_in_epic() {
  m_epic_login = std::make_unique<EpicLoginDialog>();
  if (m_connections) {
    m_epic_login->set_transient_for(*m_connections);
  } else {
    m_epic_login->set_transient_for(*this);
  }
  m_epic_login->signal_signed_in().connect([this](const EpicSession& session) {
    if (!EpicClient::save_session(session)) {
      show_notice("Could not store the Epic session in the Secret Service.", true);
    }
    m_epic_session = session;
    update_epic_status();
    request_epic_library();
  });
  m_epic_login->signal_failed().connect(
      [this](const std::string& message) { show_notice(message, true); });
  m_epic_login->present();
}

void Window::sign_out_epic() {
  EpicClient::clear_session();
  m_epic_session.reset();
  m_epic_games.clear();
  EpicClient::clear_games_cache();
  merge_epic_games();
  update_epic_status();
  show_notice("Signed out of Epic Games.");
}

void Window::update_epic_status() {
  if (m_epic_status == nullptr) {
    return;
  }
  if (!m_epic_session.has_value()) {
    m_epic_status->set_text("Not connected.");
    return;
  }
  const std::string& name = m_epic_session->display_name;
  std::string text = name.empty() ? "Connected." : "Connected as " + name + ".";
  if (!m_epic_last_error.empty()) {
    text += " Last sync failed: " + m_epic_last_error;
  } else if (m_epic_last_count == 0) {
    text += " No owned games were returned.";
  } else {
    text += " " + std::to_string(m_epic_last_count) + " owned games.";
  }
  m_epic_status->set_text(text);
}

void Window::rebuild_games() {
  m_games = load_games(m_scan, m_faugus_scan, m_store_scan, m_flags);
  const FaugusGame* epic_entry = nullptr;
  const FaugusGame* ubisoft_entry = nullptr;
  for (const FaugusGame& entry : m_faugus_scan.games) {
    if (entry.gameid == "epic-games") {
      epic_entry = &entry;
    } else if (entry.gameid == "ubisoft-connect") {
      ubisoft_entry = &entry;
    }
  }

  for (const EpicGame& owned : m_epic_games) {
    const std::string normalized = normalise_title(owned.title);
    auto found = std::find_if(
        m_games.begin(), m_games.end(), [&owned, &normalized](const Game& game) {
          return game.kind == GameKind::Store && game.source == "Epic" &&
                 (game.epic_app_name == owned.app_name ||
                  normalise_title(game.title) == normalized);
        });
    if (found != m_games.end()) {
      found->epic_app_name = owned.app_name;
      found->epic_namespace = owned.namespace_id;
      found->epic_catalog_id = owned.catalog_id;
      if (epic_entry != nullptr) {
        found->epic_launcher_path = epic_entry->path;
      }
      if (!owned.title.empty()) {
        found->title = owned.title;
      }
      if (found->cover_path.empty()) {
        found->cover_path = owned.cover_path;
      }
      if (found->hero_path.empty()) {
        found->hero_path = owned.hero_path.empty() ? owned.cover_path : owned.hero_path;
      }
      continue;
    }
    Game game;
    game.app_id = "store:epic:" + owned.app_name;
    game.epic_app_name = owned.app_name;
    game.epic_namespace = owned.namespace_id;
    game.epic_catalog_id = owned.catalog_id;
    game.store_launcher_gameid = "epic-games";
    game.title = owned.title;
    game.source = "Epic";
    game.kind = GameKind::Store;
    game.installed = false;
    game.cover_path = owned.cover_path;
    game.hero_path = owned.hero_path.empty() ? owned.cover_path : owned.hero_path;
    if (epic_entry != nullptr) {
      game.epic_launcher_path = epic_entry->path;
      game.library_path = epic_entry->prefix;
      game.runner = epic_entry->runner;
      game.launch_arguments = epic_entry->launch_arguments;
      game.executable = epic_entry->path;
    }
    game.flags = m_flags.get(game.app_id);
    m_games.push_back(std::move(game));
  }

  for (const UbisoftGame& owned : m_ubisoft_games) {
    const std::string normalized = normalise_title(owned.name);
    auto found = std::find_if(
        m_games.begin(), m_games.end(), [&owned, &normalized](const Game& game) {
          return game.kind == GameKind::Store && game.source == "Ubisoft" &&
                 (game.ubisoft_space_id == owned.space_id ||
                  normalise_title(game.title) == normalized);
        });
    if (found != m_games.end()) {
      found->ubisoft_space_id = owned.space_id;
      found->ubisoft_game_id = owned.game_id;
      if (!owned.name.empty()) {
        found->title = owned.name;
      }
      if (found->cover_path.empty()) {
        found->cover_path = owned.cover_path;
      }
      if (found->hero_path.empty()) {
        found->hero_path = owned.hero_path.empty() ? owned.cover_path : owned.hero_path;
      }
      found->playtime_minutes = owned.playtime_minutes;
      found->achievements_unlocked = owned.achievements_unlocked;
      found->achievements_total = owned.achievements_total;
      found->stats_available = owned.stats_available;
      found->achievements_known = owned.achievements_known;
      continue;
    }
    Game game;
    game.app_id = "store:ubisoft:" + owned.space_id;
    game.ubisoft_space_id = owned.space_id;
    game.ubisoft_game_id = owned.game_id;
    game.store_launcher_gameid = "ubisoft-connect";
    game.title = owned.name;
    game.source = "Ubisoft";
    game.kind = GameKind::Store;
    game.installed = false;
    game.cover_path = owned.cover_path;
    game.hero_path = owned.hero_path.empty() ? owned.cover_path : owned.hero_path;
    game.playtime_minutes = owned.playtime_minutes;
    game.achievements_unlocked = owned.achievements_unlocked;
    game.achievements_total = owned.achievements_total;
    game.stats_available = owned.stats_available;
    game.achievements_known = owned.achievements_known;
    if (ubisoft_entry != nullptr) {
      game.library_path = ubisoft_entry->prefix;
      game.runner = ubisoft_entry->runner;
      game.launch_arguments = ubisoft_entry->launch_arguments;
      game.executable = ubisoft_entry->path;
    }
    game.flags = m_flags.get(game.app_id);
    m_games.push_back(std::move(game));
  }

  for (Game& game : m_games) {
    if (game.kind != GameKind::Store) {
      continue;
    }
    if (game.source == "Epic" && !game.epic_launcher_path.empty()) {
      const std::string uri =
          epic_launch_uri(game.epic_namespace, game.epic_catalog_id, game.epic_app_name);
      if (!uri.empty()) {
        game.launch_uri = uri;
        game.install_uri =
            epic_install_uri(game.epic_namespace, game.epic_catalog_id, game.epic_app_name);
        game.store_uri = epic_store_uri(game.title);
        if (game.install_uri.empty()) {
          game.install_uri = game.store_uri;
        }
        game.executable = game.epic_launcher_path;
      }
    } else if (game.source == "Ubisoft" && !game.ubisoft_space_id.empty() &&
               !game.library_path.empty()) {
      std::string launch_id = game.ubisoft_game_id;
      const std::string prefix =
          ubisoft_entry != nullptr ? ubisoft_entry->prefix : game.library_path;
      if (!game.ubisoft_space_id.empty() &&
          (launch_id.empty() || launch_id == game.ubisoft_space_id)) {
        const std::string mapped =
            UbisoftClient::launch_id_for(prefix, game.ubisoft_space_id);
        if (!mapped.empty()) {
          launch_id = mapped;
          game.ubisoft_game_id = mapped;
        }
      }
      const std::string uri = ubisoft_launch_uri(
          launch_id.empty() ? game.ubisoft_space_id : launch_id);
      if (!uri.empty()) {
        game.launch_uri = uri;
        game.install_uri = uri;
        if (ubisoft_entry != nullptr && !ubisoft_entry->path.empty()) {
          game.executable = ubisoft_entry->path;
        }
      }
    }
  }

  const std::string ubisoft_prefix =
      ubisoft_entry != nullptr ? ubisoft_entry->prefix : std::string();
  if (!ubisoft_prefix.empty()) {
    const auto stats = UbisoftClient::load_local_stats(ubisoft_prefix);
    for (Game& game : m_games) {
      if (game.source != "Ubisoft" || game.ubisoft_space_id.empty()) {
        continue;
      }
      const auto found = stats.find(game.ubisoft_space_id);
      if (found != stats.end()) {
        game.playtime_minutes = found->second.playtime_minutes;
        game.achievements_unlocked = found->second.achievements_unlocked;
        game.achievements_total =
            UbisoftClient::achievement_total(ubisoft_prefix, game.ubisoft_space_id);
        game.achievements_known = found->second.achievements_known;
        game.stats_available = true;
      }
    }
  }

  apply_cached_covers();
  m_library.set_games(m_games);
}

void Window::merge_epic_games() { rebuild_games(); }

void Window::merge_ubisoft_games() { rebuild_games(); }

void Window::start_ubisoft_worker() {
  m_ubisoft_thread = std::jthread([this](std::stop_token stop) {
    while (!stop.stop_requested()) {
      UbisoftRequest request;
      {
        std::unique_lock lock(m_ubisoft_mutex);
        if (!m_ubisoft_cv.wait(lock, stop, [this] { return m_ubisoft_request.pending; })) {
          return;
        }
        request = m_ubisoft_request;
        m_ubisoft_request.pending = false;
      }
      UbisoftSession session = request.session;
      std::string error;
      bool refreshed = UbisoftClient::refresh(&session, &error);
      std::optional<UbisoftSession> saved;
      if (refreshed) {
        (void)UbisoftClient::save_session(session);
        saved = session;
      } else {
        session = request.session;
        error.clear();
      }
      std::vector<UbisoftGame> games;
      bool ok = UbisoftClient::fetch_owned_games(session, &games, &error);
      if (ok && !request.prefix.empty()) {
        const auto stats = UbisoftClient::load_local_stats(request.prefix);
        for (UbisoftGame& game : games) {
          const auto found = stats.find(game.space_id);
          if (found != stats.end()) {
            game.playtime_minutes = found->second.playtime_minutes;
            game.achievements_unlocked = found->second.achievements_unlocked;
            game.achievements_known = found->second.achievements_known;
            game.achievements_total =
                UbisoftClient::achievement_total(request.prefix, game.space_id);
            game.stats_available = true;
          }
        }
      }
      if (ok) {
        for (UbisoftGame& game : games) {
          if (game.stats_available && game.achievements_known) {
            continue;
          }
          UbisoftStats remote;
          std::string stats_error;
          if (UbisoftClient::fetch_remote_stats(session, game.space_id, &remote,
                                                &stats_error)) {
            ubisoft_log("stats: remote " + game.name + " play=" +
                        std::to_string(remote.playtime_minutes) + " ach=" +
                        std::to_string(remote.achievements_unlocked) + " known=" +
                        (remote.achievements_known ? "yes" : "no"));
            if (game.playtime_minutes == 0 || remote.playtime_minutes > 0) {
              game.playtime_minutes = remote.playtime_minutes;
            }
            if (remote.achievements_known) {
              game.achievements_unlocked = remote.achievements_unlocked;
              game.achievements_known = true;
            }
            game.achievements_total =
                UbisoftClient::achievement_total(request.prefix, game.space_id);
            game.stats_available = true;
          } else {
            ubisoft_log("stats: remote failed for " + game.name + " " + stats_error);
          }
          g_usleep(50000);
        }
      }
      if (ok) {
        for (UbisoftGame& game : games) {
          const std::pair<const char*, const std::string*> sources[] = {
              {"cover", &game.cover_url},
              {"banner", &game.banner_url},
              {"background", &game.background_url},
          };
          for (const auto& [kind, url] : sources) {
            if (url->empty() || !game.cover_path.empty()) {
              continue;
            }
            std::string cover_error;
            game.cover_path = artwork::fetch_image(*url, &cover_error);
            if (game.cover_path.empty()) {
              GUri* parsed = g_uri_parse(url->c_str(), G_URI_FLAGS_NONE, nullptr);
              const char* host = parsed != nullptr ? g_uri_get_host(parsed) : nullptr;
              ubisoft_log(std::string("cover: ") + kind + " failed host=" +
                          (host != nullptr ? host : "?") + " error=" + cover_error);
              if (parsed != nullptr) {
                g_uri_unref(parsed);
              }
            }
          }
          if (game.cover_path.empty()) {
            artwork::ensure_store_artwork(game.name, nullptr);
            game.cover_path = artwork::cached_store_cover(game.name);
            if (game.cover_path.empty()) {
              ubisoft_log("cover: no art for " + game.name);
            }
          }
          if (!game.background_url.empty()) {
            game.hero_path = artwork::fetch_image(game.background_url, nullptr);
          }
          if (game.hero_path.empty() && !game.banner_url.empty()) {
            game.hero_path = artwork::fetch_image(game.banner_url, nullptr);
          }
          if (game.hero_path.empty()) {
            game.hero_path = game.cover_path;
          }
        }
      }
      ubisoft_log("worker: fetched=" + std::to_string(games.size()) +
                  " error=" + (ok ? std::string() : error));
      {
        std::lock_guard lock(m_ubisoft_result_mutex);
        m_pending_ubisoft_games = std::move(games);
        m_pending_ubisoft_session = std::move(saved);
        m_pending_ubisoft_error = ok ? std::string() : error;
      }
      m_ubisoft_dispatcher.emit();
    }
  });
}

void Window::request_ubisoft_library() {
  if (!m_ubisoft_session.has_value()) {
    return;
  }
  std::string prefix;
  for (const FaugusGame& entry : m_faugus_scan.games) {
    if (entry.gameid == "ubisoft-connect") {
      prefix = entry.prefix;
      break;
    }
  }
  {
    std::lock_guard lock(m_ubisoft_mutex);
    m_ubisoft_request.session = *m_ubisoft_session;
    m_ubisoft_request.prefix = prefix;
    m_ubisoft_request.pending = true;
  }
  m_ubisoft_cv.notify_one();
}

void Window::sign_in_ubisoft() {
  m_ubisoft_login = std::make_unique<UbisoftLoginDialog>();
  if (m_connections) {
    m_ubisoft_login->set_transient_for(*m_connections);
  } else {
    m_ubisoft_login->set_transient_for(*this);
  }
  m_ubisoft_login->signal_signed_in().connect([this](const UbisoftSession& session) {
    if (!UbisoftClient::save_session(session)) {
      show_notice("Could not store the Ubisoft session in the Secret Service.", true);
    }
    m_ubisoft_session = session;
    update_ubisoft_status();
    request_ubisoft_library();
  });
  m_ubisoft_login->signal_failed().connect(
      [this](const std::string& message) { show_notice(message, true); });
  m_ubisoft_login->present();
}

void Window::sign_out_ubisoft() {
  UbisoftClient::clear_session();
  UbisoftClient::clear_games_cache();
  m_ubisoft_session.reset();
  m_ubisoft_games.clear();
  m_ubisoft_last_error.clear();
  m_ubisoft_last_count = 0;
  rebuild_games();
  update_ubisoft_status();
  show_notice("Signed out of Ubisoft Connect.");
}

void Window::update_ubisoft_status() {
  if (m_ubisoft_status == nullptr) {
    return;
  }
  if (!m_ubisoft_session.has_value()) {
    m_ubisoft_status->set_text("Not connected.");
    return;
  }
  const std::string& name = m_ubisoft_session->username;
  std::string text = name.empty() ? "Connected." : "Connected as " + name + ".";
  if (!m_ubisoft_last_error.empty()) {
    text += " Last sync failed: " + m_ubisoft_last_error;
  } else if (m_ubisoft_last_count == 0) {
    text += " No owned games were returned.";
  } else {
    text += " " + std::to_string(m_ubisoft_last_count) + " owned games.";
  }
  m_ubisoft_status->set_text(text);
}

void Window::show_about() {
  if (!m_about) {
    m_about = std::make_unique<Gtk::AboutDialog>();
    m_about->set_program_name("LGL Games Launcher");
    m_about->set_version(LGL_GAMES_LAUNCHER_VERSION);
    m_about->set_website("https://github.com/linuxgamerlife/lgl-games-launcher");
    m_about->set_website_label("Project page and issue tracker");
    m_about->set_authors({"LinuxGamerLife", "Omakade contributors"});
    m_about->set_comments(
        "A local Steam and Faugus library for Fedora.\n\nInspired by Omakade by Tyler South.\nBuilt with GTK 4 and libadwaita.");
    m_about->set_license_type(Gtk::License::GPL_3_0);
    m_about->set_logo_icon_name("lgl-games-launcher");
    m_about->set_transient_for(*this);
    m_about->set_modal(true);
    m_about->set_hide_on_close(true);
  }
  m_about->present();
}

void Window::update_dark_class() {
  auto* manager = adw_style_manager_get_default();
  if (manager != nullptr && adw_style_manager_get_dark(manager)) {
    add_css_class("dark");
  } else {
    remove_css_class("dark");
  }
}

void Window::on_style_manager_dark(GObject*, GParamSpec*, gpointer data) {
  static_cast<Window*>(data)->update_dark_class();
}

Game* Window::find_game(const std::string& app_id) {
  const auto found = std::find_if(m_games.begin(), m_games.end(), [&app_id](const Game& game) {
    return game.app_id == app_id;
  });
  return found == m_games.end() ? nullptr : &*found;
}

const Game* Window::find_game(const std::string& app_id) const {
  const auto found = std::find_if(m_games.begin(), m_games.end(), [&app_id](const Game& game) {
    return game.app_id == app_id;
  });
  return found == m_games.end() ? nullptr : &*found;
}

} // namespace gameslauncher
