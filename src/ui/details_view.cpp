#include "ui/details_view.h"

#include "core/format.h"

#include <giomm/appinfo.h>
#include <gtkmm/separator.h>
#include <gtkmm/scrolledwindow.h>

#include <filesystem>
#include <string>

namespace gameslauncher {
namespace {

Gtk::Box* make_row(const std::string& label, Gtk::Label* value) {
  auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
  row->add_css_class("detail-row");
  auto* name = Gtk::make_managed<Gtk::Label>(label);
  name->set_xalign(0.0F);
  name->set_size_request(150, -1);
  name->add_css_class("muted");
  value->set_xalign(0.0F);
  value->set_hexpand(true);
  value->set_selectable(true);
  value->set_wrap(true);
  row->append(*name);
  row->append(*value);
  return row;
}

} // namespace

DetailsView::DetailsView()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12) {
  add_css_class("body");
  set_hexpand(true);
  set_vexpand(true);

  m_back.set_label("← Back");
  m_back.add_css_class("gl-small");
  m_back.set_halign(Gtk::Align::START);
  m_back.signal_clicked().connect([this] { m_back_signal.emit(); });
  append(m_back);

  auto* scroller = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroller->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroller->set_vexpand(true);
  append(*scroller);

  m_body.set_orientation(Gtk::Orientation::VERTICAL);
  m_body.set_spacing(12);
  scroller->set_child(m_body);

  auto* top = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 18);
  m_cover.set_size_request(220, 330);
  m_cover.set_can_shrink(true);
  m_cover.set_content_fit(Gtk::ContentFit::COVER);
  m_cover.add_css_class("detail-cover");
  m_cover.set_halign(Gtk::Align::START);
  m_cover.set_valign(Gtk::Align::START);
  top->append(m_cover);

  auto* info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
  info->set_hexpand(true);
  m_title.add_css_class("detail-title");
  m_title.set_wrap(true);
  m_title.set_xalign(0.0F);
  info->append(m_title);
  m_source.add_css_class("muted");
  m_source.set_xalign(0.0F);
  info->append(m_source);

  auto* actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  m_play_button.set_label("Play");
  m_play_button.add_css_class("gl-primary");
  m_play_button.add_css_class("suggested-action");
  m_play_button.signal_clicked().connect([this] { m_play.emit(m_app_id); });
  actions->append(m_play_button);

  m_favorite_button.add_css_class("gl-small");
  m_favorite_button.signal_clicked().connect(
      [this] { m_favorite.emit(m_app_id, !m_is_favorite); });
  actions->append(m_favorite_button);

  m_hide_button.add_css_class("gl-small");
  m_hide_button.signal_clicked().connect([this] { m_hide.emit(m_app_id, !m_is_hidden); });
  actions->append(m_hide_button);

  m_store_button.set_label("Steam store");
  m_store_button.add_css_class("gl-small");
  m_store_button.signal_clicked().connect([this] {
    try {
      Gio::AppInfo::launch_default_for_uri("steam://store/" + m_app_id);
    } catch (const Glib::Error&) {
      // The notice is intentionally non-blocking; the store is optional.
    }
  });
  actions->append(m_store_button);

  m_copy_button.set_label("Copy app ID");
  m_copy_button.add_css_class("gl-small");
  m_copy_button.signal_clicked().connect([this] {
    auto clipboard = get_clipboard();
    if (clipboard) {
      clipboard->set_text(m_app_id);
    }
  });
  actions->append(m_copy_button);
  info->append(*actions);
  top->append(*info);
  m_body.append(*top);

  auto* details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
  details->add_css_class("gl-card");
  details->append(*make_row("App ID", &m_app_id_label));
  details->append(*make_row("Install path", &m_install_path));
  details->append(*make_row("Library", &m_library_path));
  details->append(*make_row("Last played", &m_last_played));
  details->append(*make_row("Playtime", &m_playtime));
  m_body.append(*details);

  auto* tags_card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  tags_card->add_css_class("gl-card");
  auto* tags_label = Gtk::make_managed<Gtk::Label>("TAGS");
  tags_label->add_css_class("section-label");
  tags_label->set_xalign(0.0F);
  tags_card->append(*tags_label);
  auto* tags_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  m_tags.set_placeholder_text("co-op, controller, backlog");
  m_tags.set_hexpand(true);
  tags_row->append(m_tags);
  m_save_tags.set_label("Save tags");
  m_save_tags.add_css_class("gl-small");
  m_save_tags.signal_clicked().connect(
      [this] { m_tags_signal.emit(m_app_id, m_tags.get_text()); });
  tags_row->append(m_save_tags);
  tags_card->append(*tags_row);
  m_body.append(*tags_card);

  auto* spacer = Gtk::make_managed<Gtk::Box>();
  spacer->set_vexpand(true);
  m_body.append(*spacer);
}

void DetailsView::set_game(const Game& game) {
  m_app_id = game.steam.app_id;
  m_is_favorite = game.flags.favorite;
  m_is_hidden = game.flags.hidden;
  m_title.set_text(game.steam.name);
  m_source.set_text(std::string("Steam") + (game.steam.installed ? " · installed" : " · missing install"));
  if (!game.steam.hero_path.empty() &&
      std::filesystem::is_regular_file(game.steam.hero_path)) {
    try {
      m_cover.set_filename(game.steam.hero_path);
    } catch (const Glib::Error&) {
      m_cover.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.svg");
    }
  } else if (!game.steam.cover_path.empty() &&
             std::filesystem::is_regular_file(game.steam.cover_path)) {
    try {
      m_cover.set_filename(game.steam.cover_path);
    } catch (const Glib::Error&) {
      m_cover.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.svg");
    }
  } else {
    m_cover.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.svg");
  }
  m_app_id_label.set_text(game.steam.app_id);
  m_install_path.set_text(game.steam.install_path);
  m_library_path.set_text(game.steam.library_path);
  m_last_played.set_text(format_last_played(game.steam.last_played));
  m_playtime.set_text(format_playtime(game.steam.playtime_minutes));
  m_tags.set_text(game.flags.tags);
  update_buttons();
}

void DetailsView::update_buttons() {
  m_favorite_button.set_label(m_is_favorite ? "Remove favourite" : "Add favourite");
  m_hide_button.set_label(m_is_hidden ? "Unhide game" : "Hide game");
}

} // namespace gameslauncher
