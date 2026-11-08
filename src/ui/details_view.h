#pragma once

#include "core/library.h"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/picture.h>
#include <sigc++/signal.h>

#include <string>

namespace gameslauncher {

class DetailsView : public Gtk::Box {
public:
  DetailsView();

  void set_game(const Game& game);
  [[nodiscard]] const std::string& current_app_id() const { return m_app_id; }

  sigc::signal<void()>& signal_back() { return m_back_signal; }
  sigc::signal<void(const std::string&)>& signal_play() { return m_play; }
  sigc::signal<void(const std::string&, bool)>& signal_favorite() { return m_favorite; }
  sigc::signal<void(const std::string&, bool)>& signal_hide() { return m_hide; }
  sigc::signal<void(const std::string&, const std::string&)>& signal_tags() {
    return m_tags_signal;
  }

private:
  void update_buttons();

  std::string m_app_id;
  bool m_is_favorite = false;
  bool m_is_hidden = false;

  Gtk::Button m_back;
  Gtk::Box m_body;
  Gtk::Picture m_cover;
  Gtk::Label m_title;
  Gtk::Label m_source;
  Gtk::Label m_app_id_label;
  Gtk::Label m_install_path;
  Gtk::Label m_library_path;
  Gtk::Label m_last_played;
  Gtk::Label m_playtime;
  Gtk::Entry m_tags;
  Gtk::Button m_save_tags;
  Gtk::Button m_play_button;
  Gtk::Button m_favorite_button;
  Gtk::Button m_hide_button;
  Gtk::Button m_store_button;
  Gtk::Button m_copy_button;

  sigc::signal<void()> m_back_signal;
  sigc::signal<void(const std::string&)> m_play;
  sigc::signal<void(const std::string&, bool)> m_favorite;
  sigc::signal<void(const std::string&, bool)> m_hide;
  sigc::signal<void(const std::string&, const std::string&)> m_tags_signal;
};

} // namespace gameslauncher
