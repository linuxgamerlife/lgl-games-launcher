#pragma once

#include "core/igdb.h"
#include "core/library.h"
#include "core/steam_achievements.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/linkbutton.h>
#include <gtkmm/overlay.h>
#include <gtkmm/picture.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <sigc++/signal.h>

#include <array>
#include <map>
#include <string>

namespace gameslauncher {

class DetailsView : public Gtk::Box {
public:
  DetailsView();

  void set_game(const Game& game);
  void update_flags(const GameFlags& flags);
  void update_artwork(const Game& game);
  void set_igdb_loading();
  void set_igdb(const IgdbInfo& info);
  void set_achievements_loading();
  void set_achievements(const SteamAchievementsResult& result);
  void set_status_message(const std::string& text);

  [[nodiscard]] const std::string& current_app_id() const { return m_app_id; }

  sigc::signal<void()>& signal_back() { return m_back_signal; }
  sigc::signal<void(const std::string&)>& signal_play() { return m_play; }
  sigc::signal<void(const std::string&, bool)>& signal_favorite() { return m_favorite; }
  sigc::signal<void(const std::string&, bool)>& signal_hide_game() { return m_hide; }
  sigc::signal<void(const std::string&)>& signal_manage() { return m_manage; }
  sigc::signal<void(const std::string&, const std::string&)>& signal_tags() {
    return m_tags_signal;
  }
  sigc::signal<void(const std::string&, const std::string&)>& signal_status() {
    return m_status_signal;
  }
  sigc::signal<void(const std::string&)>& signal_refresh_igdb() { return m_refresh_igdb; }
  sigc::signal<void(const std::string&)>& signal_refresh_achievements() {
    return m_refresh_achievements;
  }
  sigc::signal<void()>& signal_connect_igdb() { return m_connect_igdb; }

private:
  void build_hero();
  void build_details();
  void build_igdb();
  void build_achievements();
  void update_status_buttons();
  void update_buttons();
  void clear_achievements();
  Gtk::Box* make_stat_card(const std::string& label, Gtk::Label& value);

  std::string m_app_id;
  bool m_is_steam = true;
  bool m_is_favorite = false;
  bool m_is_hidden = false;
  std::string m_status;

  Gtk::ScrolledWindow m_scroller;
  Gtk::Box m_page;

  Gtk::Overlay m_hero;
  Gtk::Picture m_hero_picture;
  Gtk::Box m_hero_shade;
  Gtk::Box m_hero_left_shade;
  Gtk::Box m_hero_content;
  Gtk::Button m_back;
  Gtk::Box m_top;
  Gtk::Picture m_cover;
  Gtk::Label m_title;
  Gtk::Label m_source;
  Gtk::Label m_installed;
  Gtk::LinkButton m_protondb;
  Gtk::LinkButton m_pcgamingwiki;
  Gtk::Button m_play_button;
  Gtk::Button m_favorite_button;
  Gtk::Button m_hide_button;
  Gtk::Button m_manage_button;
  Gtk::Button m_copy_title;

  Gtk::Box m_organize_card;
  std::map<std::string, Gtk::ToggleButton*> m_status_buttons;
  Gtk::Entry m_tags;
  Gtk::Button m_save_tags;
  Gtk::Button m_new_collection;

  Gtk::Box m_stats;
  Gtk::Box* m_achievements_card = nullptr;
  Gtk::Box* m_completion_card = nullptr;
  Gtk::Label m_playtime_value;
  Gtk::Label m_achievements_value;
  Gtk::Label m_completion_value;

  Gtk::Box m_igdb_section;
  Gtk::Label m_igdb_status;
  Gtk::Button m_igdb_refresh;
  Gtk::Button m_igdb_connect;
  Gtk::Label m_igdb_critic;
  Gtk::Label m_igdb_rushed;
  Gtk::Label m_igdb_normal;
  Gtk::Label m_igdb_complete;
  Gtk::LinkButton m_igdb_link;

  Gtk::Box m_achievement_section;
  Gtk::Label m_achievement_status;
  Gtk::ProgressBar m_achievement_progress;
  Gtk::Label m_achievement_percent;
  Gtk::Label m_achievement_count;
  Gtk::Button m_achievement_refresh;
  Gtk::Grid m_achievement_grid;

  sigc::signal<void()> m_back_signal;
  sigc::signal<void(const std::string&)> m_play;
  sigc::signal<void(const std::string&, bool)> m_favorite;
  sigc::signal<void(const std::string&, bool)> m_hide;
  sigc::signal<void(const std::string&)> m_manage;
  sigc::signal<void(const std::string&, const std::string&)> m_tags_signal;
  sigc::signal<void(const std::string&, const std::string&)> m_status_signal;
  sigc::signal<void(const std::string&)> m_refresh_igdb;
  sigc::signal<void(const std::string&)> m_refresh_achievements;
  sigc::signal<void()> m_connect_igdb;
};

} // namespace gameslauncher
