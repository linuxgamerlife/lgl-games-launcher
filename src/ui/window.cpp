#include "ui/window.h"

#include "core/artwork.h"
#include "core/secret_store.h"
#include "core/format.h"
#include "core/launch.h"
#include "core/paths.h"

#include <adwaita.h>
#include <giomm/menu.h>
#include <glibmm/main.h>
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
  m_theme.signal_changed().connect([this] { update_dark_class(); });
  m_artwork_dispatcher.connect([this] {
    apply_cached_covers();
    m_library.set_games(m_games);
    if (!m_details.current_app_id().empty()) {
      if (const Game* game = find_game(m_details.current_app_id())) {
        m_details.update_artwork(*game);
      }
    }
  });
  m_igdb_dispatcher.connect([this] {
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
  m_achievements_dispatcher.connect([this] {
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
  m_details.signal_back().connect([this] { go_back(); });
  m_details.signal_play().connect([this](const std::string& app_id) { play(app_id); });
  m_details.signal_favorite().connect(
      [this](const std::string& app_id, bool value) { toggle_favorite(app_id, value); });
  m_details.signal_hide().connect(
      [this](const std::string& app_id, bool value) { toggle_hidden(app_id, value); });
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

  Glib::signal_idle().connect_once([this] { scan_and_load(); });
}

Window::~Window() {
  if (m_igdb_thread.joinable()) {
    m_igdb_thread.request_stop();
    m_igdb_thread.join();
  }
  if (m_achievements_thread.joinable()) {
    m_achievements_thread.request_stop();
    m_achievements_thread.join();
  }
  for (std::jthread& thread : m_artwork_threads) {
    if (thread.joinable()) {
      thread.request_stop();
      thread.join();
    }
  }
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

  m_refresh_button.set_icon_name("view-refresh-symbolic");
  m_refresh_button.set_tooltip_text("Rescan Steam library (Ctrl+R)");
  m_refresh_button.set_action_name("win.refresh");
  m_header.pack_end(m_refresh_button);

  auto menu = Gio::Menu::create();

  auto library_section = Gio::Menu::create();
  library_section->append("Refresh Steam library", "win.refresh");
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
  m_games = load_games(m_scan, m_flags);
  apply_cached_covers();
  m_library.set_games(m_games);
  if (!m_scan.warnings.empty()) {
    show_notice(m_scan.warnings.front(), true);
  } else if (m_games.empty()) {
    show_notice("No Steam games found. Check that Steam is installed and has a library.", true);
  }
  fetch_missing_covers();
}

void Window::apply_cached_covers() {
  for (Game& game : m_games) {
    if (!game.steam.cover_path.empty()) {
      continue;
    }
    const std::string cached = artwork::cached_cover_path(game.steam.app_id);
    if (!cached.empty()) {
      game.steam.cover_path = cached;
    }
  }
}

void Window::fetch_missing_covers() {
  std::vector<std::string> missing;
  for (const Game& game : m_games) {
    if (game.steam.cover_path.empty()) {
      missing.push_back(game.steam.app_id);
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
            artwork::fetch_steam_cover(missing[index], nullptr, nullptr);
          }
          m_artwork_dispatcher.emit();
        });
  }
}

void Window::refresh() {
  scan_and_load();
  if (m_scan.warnings.empty() && !m_games.empty()) {
    show_notice("Steam library refreshed: " + std::to_string(m_games.size()) + " games.");
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
  const bool flatpak =
      game->steam.library_path.find("/com.valvesoftware.Steam/") != std::string::npos;
  const LaunchCommand command = steam_launch_command(*game, flatpak);
  std::string error;
  if (!launch(command, &error)) {
    show_notice(error, true);
    return;
  }
  show_notice("Launching " + game->steam.name + "…");
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
  m_library.set_games(m_games);
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
  const std::string title = game != nullptr ? game->steam.name : std::string();
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
  m_igdb_thread = std::jthread([this, app_id, title, client_id, secret](std::stop_token stop) {
    IgdbClient client(client_id, secret);
    IgdbInfo info;
    std::string error;
    if (!client.fetch(app_id, title, &info, &error)) {
      info.error = error.empty() ? "IGDB data is unavailable." : error;
    }
    {
      std::lock_guard lock(m_metadata_mutex);
      m_pending_igdb_app = app_id;
      m_pending_igdb = info;
    }
    if (!stop.stop_requested()) {
      m_igdb_dispatcher.emit();
    }
  });
}

void Window::load_achievements(const std::string& app_id) {
  if (app_id.empty()) {
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
  m_achievements_thread = std::jthread(
      [this, app_id, steam_id](std::stop_token stop) {
        SteamAchievementsResult result = fetch_steam_achievements(steam_id, app_id);
        {
          std::lock_guard lock(m_metadata_mutex);
          m_pending_achievements_app = app_id;
          m_pending_achievements = result;
        }
        if (!stop.stop_requested()) {
          m_achievements_dispatcher.emit();
        }
      });
}

void Window::show_connections() {
  if (!m_connections) {
    m_connections = std::make_unique<Gtk::Dialog>();
    m_connections->set_title("Connections");
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
  m_connections->present();
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
        "A local Steam library for Fedora.\n\nInspired by Omakade by Tyler South.\nBuilt with GTK 4 and libadwaita.");
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
    return game.steam.app_id == app_id;
  });
  return found == m_games.end() ? nullptr : &*found;
}

const Game* Window::find_game(const std::string& app_id) const {
  const auto found = std::find_if(m_games.begin(), m_games.end(), [&app_id](const Game& game) {
    return game.steam.app_id == app_id;
  });
  return found == m_games.end() ? nullptr : &*found;
}

} // namespace gameslauncher
