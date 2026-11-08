#pragma once

#include "core/library.h"

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/gridview.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/stack.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/togglebutton.h>
#include <sigc++/signal.h>

#include <map>
#include <string>
#include <vector>

namespace gameslauncher {

class LibraryView : public Gtk::Box {
public:
  LibraryView();

  void set_games(std::vector<Game> games);
  void set_sort_mode(const std::string& sort);
  void set_show_hidden(bool value);
  void refresh();
  void set_cover_size(int pixels);
  void focus_search();

  sigc::signal<void(const std::string&)>& signal_open_game() { return m_open_game; }
  sigc::signal<void(const std::string&)>& signal_sort_changed() { return m_sort_changed; }
  sigc::signal<void(bool)>& signal_show_hidden_changed() { return m_show_hidden_changed; }

private:
  void apply();
  void rebuild_model();

  std::vector<Game> m_games;
  std::map<std::string, Game> m_by_id;
  std::string m_sort = "title";
  bool m_show_hidden = false;
  bool m_favorites_only = false;
  int m_cover_size = 160;

  Gtk::Box m_filter_bar;
  Gtk::SearchEntry m_search;
  Gtk::ToggleButton m_favorites;
  Gtk::CheckButton m_show_hidden_button;
  Gtk::DropDown m_sort_dropdown;
  Glib::RefPtr<Gtk::StringList> m_sort_model;
  Gtk::Stack m_stack;
  Gtk::ScrolledWindow m_scroller;
  Gtk::GridView m_grid;
  Glib::RefPtr<Gtk::StringList> m_model;
  Glib::RefPtr<Gtk::SingleSelection> m_selection;
  Gtk::Label m_empty;
  sigc::signal<void(const std::string&)> m_open_game;
  sigc::signal<void(const std::string&)> m_sort_changed;
  sigc::signal<void(bool)> m_show_hidden_changed;
};

} // namespace gameslauncher
