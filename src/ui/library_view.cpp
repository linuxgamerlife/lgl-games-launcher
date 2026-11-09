#include "ui/library_view.h"

#include "core/format.h"

#include <gtkmm/image.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listitemfactory.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/picture.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/stringobject.h>
#include <pangomm/layout.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace gameslauncher {
namespace {

class GameCard final : public Gtk::Box {
public:
  GameCard() : Gtk::Box(Gtk::Orientation::VERTICAL, 6) {
    add_css_class("game-card");
    set_halign(Gtk::Align::CENTER);
    m_cover_frame.add_css_class("cover-frame");
    m_cover_frame.set_overflow(Gtk::Overflow::HIDDEN);
    m_cover_frame.append(m_picture);
    m_picture.set_can_shrink(true);
    m_picture.set_content_fit(Gtk::ContentFit::COVER);
    m_picture.set_halign(Gtk::Align::FILL);
    m_picture.set_valign(Gtk::Align::FILL);
    append(m_cover_frame);
    m_title.set_justify(Gtk::Justification::CENTER);
    m_title.set_wrap(true);
    m_title.set_lines(2);
    m_title.set_ellipsize(Pango::EllipsizeMode::END);
    m_title.set_max_width_chars(18);
    m_title.add_css_class("cover-title");
    append(m_title);
    m_meta.set_justify(Gtk::Justification::CENTER);
    m_meta.add_css_class("muted");
    m_meta.add_css_class("small");
    append(m_meta);
    m_favorite.set_halign(Gtk::Align::CENTER);
    m_favorite.add_css_class("small");
    append(m_favorite);
  }

  void set_game(const Game& game, int cover_size) {
    const int height = cover_size * 3 / 2;
    m_cover_frame.set_size_request(cover_size, height);
    m_picture.set_size_request(cover_size, height);
    const bool has_cover =
        !game.steam.cover_path.empty() &&
        std::filesystem::is_regular_file(game.steam.cover_path);
    if (has_cover) {
      try {
        m_picture.set_filename(game.steam.cover_path);
      } catch (const Glib::Error&) {
        m_picture.set_resource("/com/linuxgamerlife/gameslauncher/icons/no-art.png");
      }
    } else {
      m_picture.set_resource("/com/linuxgamerlife/gameslauncher/icons/no-art.png");
    }
    m_title.set_text(game.steam.name);
    std::string meta = "Steam";
    if (game.steam.playtime_minutes > 0) {
      meta += " · " + format_playtime(game.steam.playtime_minutes);
    }
    m_meta.set_text(meta);
    m_favorite.set_text(game.flags.favorite ? "★" : "");
    set_tooltip_text(game.steam.name);
    set_size_request(cover_size + 12, -1);
  }

private:
  Gtk::Box m_cover_frame;
  Gtk::Picture m_picture;
  Gtk::Label m_title;
  Gtk::Label m_meta;
  Gtk::Label m_favorite;
};

} // namespace

LibraryView::LibraryView()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12) {
  add_css_class("body");
  set_hexpand(true);
  set_vexpand(true);

  m_filter_bar.set_orientation(Gtk::Orientation::HORIZONTAL);
  m_filter_bar.set_spacing(8);
  m_filter_bar.add_css_class("filter-bar");

  m_search.set_placeholder_text("Search Steam games…");
  m_search.set_hexpand(true);
  m_search.add_css_class("search-entry");
  m_search.signal_search_changed().connect([this] { apply(); });
  m_filter_bar.append(m_search);

  m_favorites.set_label("Favourites");
  m_favorites.add_css_class("gl-small");
  m_favorites.signal_toggled().connect([this] {
    m_favorites_only = m_favorites.get_active();
    apply();
  });
  m_filter_bar.append(m_favorites);

  m_show_hidden_button.set_label("Show hidden");
  m_show_hidden_button.add_css_class("small");
  m_show_hidden_button.signal_toggled().connect([this] {
    m_show_hidden = m_show_hidden_button.get_active();
    m_show_hidden_changed.emit(m_show_hidden);
    apply();
  });
  m_filter_bar.append(m_show_hidden_button);

  m_sort_model = Gtk::StringList::create({"Title", "Recently played", "Playtime"});
  m_sort_dropdown.set_model(m_sort_model);
  m_sort_dropdown.set_selected(0);
  m_sort_dropdown.set_tooltip_text("Sort games");
  m_sort_dropdown.property_selected().signal_changed().connect([this] {
    const guint selected = m_sort_dropdown.get_selected();
    m_sort = selected == 1 ? "recent" : (selected == 2 ? "playtime" : "title");
    m_sort_changed.emit(m_sort);
    apply();
  });
  m_filter_bar.append(m_sort_dropdown);
  append(m_filter_bar);

  m_grid.set_min_columns(2);
  m_grid.set_max_columns(12);
  m_grid.set_single_click_activate(true);
  m_grid.set_vexpand(true);
  m_grid.add_css_class("library-grid");

  auto factory = Gtk::SignalListItemFactory::create();
  factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto* card = Gtk::make_managed<GameCard>();
    item->set_child(*card);
  });
  factory->signal_bind().connect([this](const Glib::RefPtr<Gtk::ListItem>& item) {
    auto* card = dynamic_cast<GameCard*>(item->get_child());
    auto object = std::dynamic_pointer_cast<Gtk::StringObject>(item->get_item());
    if (card == nullptr || object == nullptr) {
      return;
    }
    const auto found = m_by_id.find(object->get_string());
    if (found == m_by_id.end()) {
      return;
    }
    card->set_game(found->second, m_cover_size);
  });
  m_grid.set_factory(factory);
  m_grid.signal_activate().connect([this](guint position) {
    if (m_model == nullptr || position >= m_model->get_n_items()) {
      return;
    }
    m_open_game.emit(m_model->get_string(position));
  });

  m_model = Gtk::StringList::create();
  m_selection = Gtk::SingleSelection::create(m_model);
  m_grid.set_model(m_selection);

  m_scroller.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
  m_scroller.set_vexpand(true);
  m_scroller.set_child(m_grid);
  m_scroller.add_css_class("gl-scroller");

  m_empty.set_text("No Steam games found.\nCheck that Steam is installed and that this account has a library.");
  m_empty.set_wrap(true);
  m_empty.set_justify(Gtk::Justification::CENTER);
  m_empty.add_css_class("muted");
  m_empty.add_css_class("empty-state");
  m_empty.set_vexpand(true);
  m_empty.set_valign(Gtk::Align::CENTER);

  m_stack.set_vexpand(true);
  m_stack.add(m_scroller, "grid");
  m_stack.add(m_empty, "empty");
  append(m_stack);
}

void LibraryView::set_games(std::vector<Game> games) {
  m_games = std::move(games);
  m_by_id.clear();
  for (const Game& game : m_games) {
    m_by_id[game.steam.app_id] = game;
  }
  apply();
}

void LibraryView::set_sort_mode(const std::string& sort) {
  m_sort = sort;
  const guint index = sort == "recent" ? 1 : (sort == "playtime" ? 2 : 0);
  m_sort_dropdown.set_selected(index);
  apply();
}

void LibraryView::set_show_hidden(bool value) {
  m_show_hidden = value;
  m_show_hidden_button.set_active(value);
  apply();
}

void LibraryView::set_cover_size(int pixels) {
  m_cover_size = std::clamp(pixels, 96, 260);
  rebuild_model();
}

void LibraryView::refresh() { apply(); }

void LibraryView::focus_search() { m_search.grab_focus(); }

void LibraryView::apply() {
  LibraryFilter filter;
  filter.search = m_search.get_text();
  filter.sort = m_sort;
  filter.favorites_only = m_favorites_only;
  filter.show_hidden = m_show_hidden;
  const auto visible = filter_games(m_games, filter);

  std::vector<Glib::ustring> ids;
  ids.reserve(visible.size());
  for (const Game& game : visible) {
    ids.emplace_back(game.steam.app_id);
  }
  m_model = Gtk::StringList::create(ids);
  m_selection->set_model(m_model);
  if (visible.empty()) {
    m_stack.set_visible_child(m_empty);
  } else {
    m_stack.set_visible_child(m_scroller);
  }
}

void LibraryView::rebuild_model() {
  if (m_model == nullptr) {
    return;
  }
  m_selection->set_model(nullptr);
  m_selection->set_model(m_model);
}

} // namespace gameslauncher
