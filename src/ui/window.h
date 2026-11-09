#pragma once

#include "core/epic.h"
#include "core/ubisoft.h"
#include "core/flags_store.h"
#include "core/igdb.h"
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
#include <optional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace gameslauncher {

class EpicLoginDialog;
class UbisoftLoginDialog;

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
  void start_metadata_workers();
  void start_epic_worker();
  void request_epic_library();
  void sign_in_epic();
  void sign_out_epic();
  void update_epic_status();
  void merge_epic_games();
  void start_ubisoft_worker();
  void request_ubisoft_library();
  void sign_in_ubisoft();
  void sign_out_ubisoft();
  void update_ubisoft_status();
  void merge_ubisoft_games();
  void rebuild_games();
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
  FaugusScanResult m_faugus_scan;
  StoreScanResult m_store_scan;
  std::vector<Game> m_games;
  ThemeWatcher m_theme;
  sigc::connection m_theme_connection;

  bool m_system_title_bar = false;
  Glib::RefPtr<Gio::SimpleAction> m_title_action;
  gulong m_dark_handler = 0;
  sigc::connection m_notice_timeout;
  sigc::connection m_scan_idle;
  Glib::Dispatcher m_artwork_dispatcher;
  sigc::connection m_artwork_connection;
  std::vector<std::jthread> m_artwork_threads;
  std::jthread m_igdb_thread;
  std::jthread m_achievements_thread;
  std::mutex m_igdb_mutex;
  std::condition_variable_any m_igdb_cv;
  struct IgdbRequest {
    std::string app_id;
    std::string steam_app_id;
    std::string title;
    std::string client_id;
    std::string client_secret;
    bool pending = false;
  };
  IgdbRequest m_igdb_request;
  std::mutex m_achievements_mutex;
  std::condition_variable_any m_achievements_cv;
  struct AchievementRequest {
    std::string app_id;
    std::string steam_id;
    bool pending = false;
  };
  AchievementRequest m_achievements_request;
  Glib::Dispatcher m_igdb_dispatcher;
  sigc::connection m_igdb_connection;
  Glib::Dispatcher m_achievements_dispatcher;
  sigc::connection m_achievements_connection;
  std::mutex m_metadata_mutex;
  std::string m_pending_igdb_app;
  IgdbInfo m_pending_igdb;
  std::string m_pending_achievements_app;
  SteamAchievementsResult m_pending_achievements;
  std::unique_ptr<Gtk::Dialog> m_connections;
  std::optional<EpicSession> m_epic_session;
  std::string m_epic_last_error;
  std::size_t m_epic_last_count = 0;
  std::optional<UbisoftSession> m_ubisoft_session;
  std::vector<UbisoftGame> m_ubisoft_games;
  std::unique_ptr<UbisoftLoginDialog> m_ubisoft_login;
  Gtk::Label* m_ubisoft_status = nullptr;
  std::string m_ubisoft_last_error;
  std::size_t m_ubisoft_last_count = 0;
  std::jthread m_ubisoft_thread;
  std::mutex m_ubisoft_mutex;
  std::condition_variable_any m_ubisoft_cv;
  struct UbisoftRequest {
    UbisoftSession session;
    std::string prefix;
    bool pending = false;
  };
  UbisoftRequest m_ubisoft_request;
  Glib::Dispatcher m_ubisoft_dispatcher;
  sigc::connection m_ubisoft_connection;
  std::mutex m_ubisoft_result_mutex;
  std::vector<UbisoftGame> m_pending_ubisoft_games;
  std::optional<UbisoftSession> m_pending_ubisoft_session;
  std::string m_pending_ubisoft_error;
  std::vector<EpicGame> m_epic_games;
  std::unique_ptr<EpicLoginDialog> m_epic_login;
  Gtk::Label* m_epic_status = nullptr;
  std::jthread m_epic_thread;
  std::mutex m_epic_mutex;
  std::condition_variable_any m_epic_cv;
  struct EpicRequest {
    EpicSession session;
    bool pending = false;
  };
  EpicRequest m_epic_request;
  Glib::Dispatcher m_epic_dispatcher;
  sigc::connection m_epic_connection;
  std::mutex m_epic_result_mutex;
  std::vector<EpicGame> m_pending_epic_games;
  std::optional<EpicSession> m_pending_epic_session;
  std::string m_pending_epic_error;
  Gtk::Entry* m_igdb_client_id_entry = nullptr;
  Gtk::Entry* m_igdb_secret_entry = nullptr;
};

} // namespace gameslauncher
