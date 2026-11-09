#pragma once

#include "core/flags_store.h"
#include "core/igdb.h"
#include "core/library.h"
#include "core/library.h"
#include "core/settings.h"
#include "core/steam_achievements.h"
#include "core/steam.h"
#include "ui/details_view.h"
#include "ui/library_view.h"
#include "ui/themewatcher.h"

#include <giomm/simpleaction.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/application.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/overlay.h>
#include <gtkmm/revealer.h>
#include <gtkmm/stack.h>

#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace gameslauncher {

class Window final : public Gtk::ApplicationWindow {
public:
  explicit Window(const Glib::RefPtr<Gtk::Application>& application);
  ~Window() override;

  void open_game(const std::string& app_id);
  void refresh();
  void go_back();
  void go_forward();

private:
  void build_header();
  void apply_title_bar(bool system, bool runtime);
  void scan_and_load();
  void fetch_missing_covers();
  void apply_cached_covers();
  void show_notice(const std::string& text, bool error = false, bool sticky = false);
  void show_game(const std::string& app_id);
  void navigate_history();
  void play(const std::string& app_id);
  void set_status(const std::string& app_id, const std::string& status);
  void open_in_steam(const std::string& app_id);
  void load_igdb(const std::string& app_id);
  void load_achievements(const std::string& app_id);
  void show_connections();
  void toggle_favorite(const std::string& app_id, bool favorite);
  void toggle_hidden(const std::string& app_id, bool hidden);
  void set_tags(const std::string& app_id, const std::string& tags);
  void update_game_flags(const std::string& app_id, const GameFlags& flags);
  void show_about();
  void update_dark_class();
  [[nodiscard]] Game* find_game(const std::string& app_id);
  [[nodiscard]] const Game* find_game(const std::string& app_id) const;

  static void on_style_manager_dark(GObject* object, GParamSpec* spec, gpointer data);

  Gtk::HeaderBar m_header;
  Gtk::Box m_logo;
  Gtk::Image m_icon;
  Gtk::Label m_wordmark;
  Gtk::Button m_home_button;
  Gtk::Button m_refresh_button;
  Gtk::MenuButton m_menu_button;
  Gtk::Box m_content;
  Gtk::Overlay m_overlay;
  Gtk::Revealer m_notice_revealer;
  Gtk::Label m_notice;
  Gtk::Stack m_stack;
  LibraryView m_library;
  DetailsView m_details;
  std::unique_ptr<Gtk::AboutDialog> m_about;
  std::vector<std::string> m_history{"library"};
  std::size_t m_history_pos = 0;

  Settings m_settings;
  FlagsStore m_flags;
  SteamScanResult m_scan;
  std::vector<Game> m_games;
  ThemeWatcher m_theme;

  bool m_system_title_bar = false;
  Glib::RefPtr<Gio::SimpleAction> m_title_action;
  gulong m_dark_handler = 0;
  sigc::connection m_notice_timeout;
  Glib::Dispatcher m_artwork_dispatcher;
  std::vector<std::jthread> m_artwork_threads;
  std::jthread m_igdb_thread;
  std::jthread m_achievements_thread;
  Glib::Dispatcher m_igdb_dispatcher;
  Glib::Dispatcher m_achievements_dispatcher;
  std::mutex m_metadata_mutex;
  std::string m_pending_igdb_app;
  IgdbInfo m_pending_igdb;
  std::string m_pending_achievements_app;
  SteamAchievementsResult m_pending_achievements;
  std::unique_ptr<Gtk::Dialog> m_connections;
  Gtk::Entry* m_igdb_client_id_entry = nullptr;
  Gtk::Entry* m_igdb_secret_entry = nullptr;
};

} // namespace gameslauncher
