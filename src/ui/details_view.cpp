#include "ui/details_view.h"

#include "core/format.h"

#include <giomm/appinfo.h>
#include <glib.h>
#include <gtkmm/image.h>
#include <gtkmm/separator.h>

#include <filesystem>
#include <string>

namespace gameslauncher {
namespace {

std::string escape_uri(const std::string& value) {
  gchar* escaped = g_uri_escape_string(value.c_str(), nullptr, false);
  std::string result = escaped != nullptr ? escaped : "";
  g_free(escaped);
  return result;
}

} // namespace

DetailsView::DetailsView() : Gtk::Box(Gtk::Orientation::VERTICAL) {
  set_hexpand(true);
  set_vexpand(true);

  m_scroller.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  m_scroller.set_vexpand(true);
  append(m_scroller);

  m_page.set_orientation(Gtk::Orientation::VERTICAL);
  m_scroller.set_child(m_page);

  build_hero();
  m_page.append(m_hero);

  auto* body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 14);
  body->set_margin(24);
  body->set_margin_top(0);
  m_page.append(*body);

  m_stats.set_orientation(Gtk::Orientation::HORIZONTAL);
  m_stats.set_spacing(12);
  m_stats.set_homogeneous(true);
  m_stats.append(*make_stat_card("PLAYTIME", m_playtime_value));
  m_stats.append(*make_stat_card("ACHIEVEMENTS", m_achievements_value));
  m_stats.append(*make_stat_card("COMPLETION", m_completion_value));
  body->append(m_stats);

  build_igdb();
  body->append(m_igdb_section);

  build_achievements();
  body->append(m_achievement_section);
}

Gtk::Box* DetailsView::make_stat_card(const std::string& label, Gtk::Label& value) {
  auto* card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  card->add_css_class("gl-card");
  card->add_css_class("stat-card");
  auto* caption = Gtk::make_managed<Gtk::Label>(label);
  caption->set_xalign(0.0F);
  caption->add_css_class("section-label");
  card->append(*caption);
  value.set_text("—");
  value.set_xalign(0.0F);
  value.add_css_class("stat-value");
  card->append(value);
  return card;
}

void DetailsView::build_hero() {
  m_hero_picture.set_size_request(-1, 650);
  m_hero_picture.set_can_shrink(true);
  m_hero_picture.set_content_fit(Gtk::ContentFit::COVER);
  m_hero_picture.add_css_class("detail-hero-picture");
  m_hero.set_child(m_hero_picture);

  m_hero_shade.add_css_class("detail-hero-shade");
  m_hero_shade.set_can_target(false);
  m_hero.add_overlay(m_hero_shade);

  m_hero_content.set_orientation(Gtk::Orientation::VERTICAL);
  m_hero_content.set_spacing(16);
  m_hero_content.set_margin(24);
  m_hero_content.add_css_class("detail-hero-content");
  m_hero.add_overlay(m_hero_content);

  m_back.set_label("← BACK");
  m_back.add_css_class("gl-small");
  m_back.add_css_class("hero-back");
  m_back.set_halign(Gtk::Align::START);
  m_back.signal_clicked().connect([this] { m_back_signal.emit(); });
  m_hero_content.append(m_back);

  m_top.set_orientation(Gtk::Orientation::HORIZONTAL);
  m_top.set_spacing(24);
  m_hero_content.append(m_top);

  auto* cover_frame = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
  cover_frame->set_size_request(280, 420);
  cover_frame->set_hexpand(false);
  cover_frame->set_vexpand(false);
  cover_frame->set_halign(Gtk::Align::START);
  cover_frame->set_valign(Gtk::Align::START);
  cover_frame->set_overflow(Gtk::Overflow::HIDDEN);
  cover_frame->add_css_class("detail-cover");
  m_cover.set_can_shrink(true);
  m_cover.set_content_fit(Gtk::ContentFit::COVER);
  m_cover.set_size_request(280, 420);
  m_cover.set_hexpand(false);
  m_cover.set_vexpand(false);
  m_cover.set_halign(Gtk::Align::FILL);
  m_cover.set_valign(Gtk::Align::FILL);
  cover_frame->append(m_cover);
  m_top.append(*cover_frame);

  auto* info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
  info->set_hexpand(true);
  info->set_valign(Gtk::Align::START);
  m_top.append(*info);

  m_title.add_css_class("detail-title");
  m_title.set_xalign(0.0F);
  m_title.set_wrap(true);
  m_title.set_selectable(true);
  info->append(m_title);

  auto* source_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  m_source.add_css_class("source-label");
  source_row->append(m_source);
  info->append(*source_row);

  auto* badges = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  m_protondb.set_label("PROTONDB");
  m_protondb.add_css_class("gl-badge");
  m_protondb.add_css_class("protondb");
  badges->append(m_protondb);
  m_pcgamingwiki.set_label("PCGAMINGWIKI");
  m_pcgamingwiki.add_css_class("gl-badge");
  badges->append(m_pcgamingwiki);
  info->append(*badges);

  m_installed.add_css_class("muted");
  m_installed.set_xalign(0.0F);
  info->append(m_installed);

  auto* actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  m_play_button.set_label("▶  PLAY");
  m_play_button.add_css_class("gl-primary");
  m_play_button.add_css_class("suggested-action");
  m_play_button.signal_clicked().connect([this] { m_play.emit(m_app_id); });
  actions->append(m_play_button);

  m_favorite_button.add_css_class("gl-small");
  m_favorite_button.signal_clicked().connect(
      [this] { m_favorite.emit(m_app_id, !m_is_favorite); });
  actions->append(m_favorite_button);

  m_manage_button.set_label("MANAGE IN STEAM");
  m_manage_button.add_css_class("gl-small");
  m_manage_button.signal_clicked().connect([this] { m_manage.emit(m_app_id); });
  actions->append(m_manage_button);

  m_copy_title.set_label("COPY TITLE");
  m_copy_title.add_css_class("gl-small");
  m_copy_title.signal_clicked().connect([this] {
    auto clipboard = get_clipboard();
    if (clipboard) {
      clipboard->set_text(m_title.get_text());
    }
  });
  actions->append(m_copy_title);

  m_hide_button.add_css_class("gl-small");
  m_hide_button.signal_clicked().connect([this] { m_hide.emit(m_app_id, !m_is_hidden); });
  actions->append(m_hide_button);
  info->append(*actions);

  m_organize_card.set_orientation(Gtk::Orientation::VERTICAL);
  m_organize_card.set_spacing(8);
  m_organize_card.add_css_class("gl-card");
  auto* organize_label = Gtk::make_managed<Gtk::Label>("ORGANIZE");
  organize_label->add_css_class("section-label");
  organize_label->set_xalign(0.0F);
  m_organize_card.append(*organize_label);

  auto* status_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  Gtk::ToggleButton* first = nullptr;
  for (const auto& [key, label] :
       std::vector<std::pair<std::string, std::string>>{{"backlog", "BACKLOG"},
                                                        {"playing", "PLAYING"},
                                                        {"completed", "COMPLETED"},
                                                        {"abandoned", "ABANDONED"}}) {
    auto* button = Gtk::make_managed<Gtk::ToggleButton>(label);
    button->add_css_class("gl-small");
    if (first == nullptr) {
      first = button;
    } else {
      button->set_group(*first);
    }
    button->signal_toggled().connect([this, key, button] {
      if (button->get_active() && m_status != key) {
        m_status = key;
        m_status_signal.emit(m_app_id, key);
      }
    });
    m_status_buttons[key] = button;
    status_row->append(*button);
  }
  m_organize_card.append(*status_row);

  auto* tags_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto* tags_label = Gtk::make_managed<Gtk::Label>("TAGS");
  tags_label->add_css_class("section-label");
  tags_label->set_size_request(70, -1);
  tags_row->append(*tags_label);
  m_tags.set_placeholder_text("Co-op, cozy, difficult");
  m_tags.set_hexpand(true);
  tags_row->append(m_tags);
  m_save_tags.set_label("SAVE");
  m_save_tags.add_css_class("gl-small");
  m_save_tags.signal_clicked().connect(
      [this] { m_tags_signal.emit(m_app_id, m_tags.get_text()); });
  tags_row->append(m_save_tags);
  m_organize_card.append(*tags_row);

  auto* collections_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto* collections_label = Gtk::make_managed<Gtk::Label>("COLLECTIONS");
  collections_label->add_css_class("section-label");
  collections_label->set_size_request(100, -1);
  collections_row->append(*collections_label);
  m_new_collection.set_label("+ NEW COLLECTION");
  m_new_collection.add_css_class("gl-small");
  m_new_collection.set_sensitive(false);
  m_new_collection.set_tooltip_text("Collections are not part of this MVP yet.");
  collections_row->append(m_new_collection);
  m_organize_card.append(*collections_row);

  info->append(m_organize_card);
}

void DetailsView::build_igdb() {
  m_igdb_section.set_orientation(Gtk::Orientation::VERTICAL);
  m_igdb_section.set_spacing(8);
  m_igdb_section.add_css_class("detail-section");

  auto* header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto* label = Gtk::make_managed<Gtk::Label>("GAME INSIGHTS · IGDB");
  label->add_css_class("section-label");
  label->set_hexpand(true);
  label->set_xalign(0.0F);
  header->append(*label);
  m_igdb_connect.set_label("CONNECT IGDB");
  m_igdb_connect.add_css_class("gl-small");
  m_igdb_connect.signal_clicked().connect([this] { m_connect_igdb.emit(); });
  header->append(m_igdb_connect);
  m_igdb_refresh.set_label("REFRESH");
  m_igdb_refresh.add_css_class("gl-small");
  m_igdb_refresh.signal_clicked().connect(
      [this] { m_refresh_igdb.emit(m_app_id); });
  header->append(m_igdb_refresh);
  m_igdb_section.append(*header);

  m_igdb_status.add_css_class("muted");
  m_igdb_status.add_css_class("small");
  m_igdb_status.set_xalign(0.0F);
  m_igdb_section.append(m_igdb_status);

  auto* cards = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
  cards->set_homogeneous(true);
  auto add_card = [cards](const std::string& caption, Gtk::Label& value) {
    auto* card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    card->add_css_class("gl-card");
    card->add_css_class("stat-card");
    auto* name = Gtk::make_managed<Gtk::Label>(caption);
    name->set_xalign(0.0F);
    name->add_css_class("section-label");
    card->append(*name);
    value.set_text("—");
    value.set_xalign(0.0F);
    value.add_css_class("stat-value");
    card->append(value);
    cards->append(*card);
  };
  add_card("IGDB CRITIC", m_igdb_critic);
  add_card("RUSHED", m_igdb_rushed);
  add_card("MAIN + EXTRAS", m_igdb_normal);
  add_card("COMPLETIONIST", m_igdb_complete);
  m_igdb_section.append(*cards);

  m_igdb_link.set_label("Open on IGDB");
  m_igdb_link.add_css_class("small");
  m_igdb_link.set_halign(Gtk::Align::START);
  m_igdb_link.set_visible(false);
  m_igdb_section.append(m_igdb_link);
}

void DetailsView::build_achievements() {
  m_achievement_section.set_orientation(Gtk::Orientation::VERTICAL);
  m_achievement_section.set_spacing(8);
  m_achievement_section.add_css_class("detail-section");

  auto* progress_header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto* progress_label = Gtk::make_managed<Gtk::Label>("ACHIEVEMENT PROGRESS");
  progress_label->add_css_class("section-label");
  progress_label->set_hexpand(true);
  progress_label->set_xalign(0.0F);
  progress_header->append(*progress_label);
  m_achievement_percent.add_css_class("muted");
  progress_header->append(m_achievement_percent);
  m_achievement_section.append(*progress_header);

  m_achievement_progress.set_show_text(false);
  m_achievement_progress.set_fraction(0.0);
  m_achievement_section.append(m_achievement_progress);

  auto* header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto* label = Gtk::make_managed<Gtk::Label>("ACHIEVEMENTS");
  label->add_css_class("section-label");
  label->set_hexpand(true);
  label->set_xalign(0.0F);
  header->append(*label);
  m_achievement_count.add_css_class("muted");
  header->append(m_achievement_count);
  m_achievement_refresh.set_label("REFRESH STEAM");
  m_achievement_refresh.add_css_class("gl-small");
  m_achievement_refresh.signal_clicked().connect(
      [this] { m_refresh_achievements.emit(m_app_id); });
  header->append(m_achievement_refresh);
  m_achievement_section.append(*header);

  m_achievement_status.add_css_class("muted");
  m_achievement_status.add_css_class("small");
  m_achievement_status.set_xalign(0.0F);
  m_achievement_section.append(m_achievement_status);

  m_achievement_grid.set_column_spacing(12);
  m_achievement_grid.set_row_spacing(8);
  m_achievement_section.append(m_achievement_grid);
}

void DetailsView::set_game(const Game& game) {
  m_app_id = game.steam.app_id;
  m_title.set_text(game.steam.name);
  m_source.set_text("STEAM");
  m_installed.set_text(game.steam.installed
                           ? "Installed locally through Steam."
                           : "Not installed. Play opens Steam's install prompt.");
  m_protondb.set_uri("https://www.protondb.com/app/" + m_app_id);
  m_pcgamingwiki.set_uri("https://www.pcgamingwiki.com/w/index.php?search=" +
                         escape_uri(game.steam.name));
  m_playtime_value.set_text(format_playtime(game.steam.playtime_minutes));
  m_play_button.set_label(game.steam.installed ? "▶  PLAY" : "INSTALL IN STEAM");
  update_flags(game.flags);

  update_artwork(game);

  m_igdb_status.set_text("IGDB data loads when a game is opened.");
  m_igdb_critic.set_text("—");
  m_igdb_rushed.set_text("—");
  m_igdb_normal.set_text("—");
  m_igdb_complete.set_text("—");
  m_igdb_link.set_visible(false);
  m_achievements_value.set_text("—");
  m_completion_value.set_text("—");
  m_achievement_status.set_text("Steam achievements load when a game is opened.");
  m_achievement_count.set_text("");
  m_achievement_percent.set_text("");
  m_achievement_progress.set_fraction(0.0);
  clear_achievements();
}

void DetailsView::update_artwork(const Game& game) {
  std::string hero = game.steam.hero_path;
  if (hero.empty() || !std::filesystem::is_regular_file(hero)) {
    hero = game.steam.cover_path;
  }
  std::string cover = game.steam.cover_path;
  if (cover.empty() || !std::filesystem::is_regular_file(cover)) {
    cover = hero;
  }
  if (!hero.empty() && std::filesystem::is_regular_file(hero)) {
    try {
      m_hero_picture.set_filename(hero);
    } catch (const Glib::Error&) {
      m_hero_picture.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.png");
    }
  } else {
    m_hero_picture.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.png");
  }
  if (!cover.empty() && std::filesystem::is_regular_file(cover)) {
    try {
      m_cover.set_filename(cover);
    } catch (const Glib::Error&) {
      m_cover.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.png");
    }
  } else {
    m_cover.set_resource("/com/linuxgamerlife/gameslauncher/icons/lgl-games-launcher.png");
  }
}

void DetailsView::update_flags(const GameFlags& flags) {
  m_is_favorite = flags.favorite;
  m_is_hidden = flags.hidden;
  m_status = flags.status;
  m_tags.set_text(flags.tags);
  update_status_buttons();
  update_buttons();
}

void DetailsView::update_status_buttons() {
  for (const auto& [key, button] : m_status_buttons) {
    button->set_active(key == m_status);
  }
}

void DetailsView::update_buttons() {
  m_favorite_button.set_label(m_is_favorite ? "♥ FAVOURITED" : "♡ ADD FAVORITE");
  m_hide_button.set_label(m_is_hidden ? "UNHIDE" : "HIDE");
}

void DetailsView::set_igdb_loading() {
  m_igdb_status.set_text("Loading from IGDB…");
  m_igdb_critic.set_text("…");
  m_igdb_rushed.set_text("…");
  m_igdb_normal.set_text("…");
  m_igdb_complete.set_text("…");
  m_igdb_connect.set_visible(false);
}

void DetailsView::set_igdb(const IgdbInfo& info) {
  if (!info.available) {
    m_igdb_status.set_text(info.error.empty() ? "IGDB is not connected." : info.error);
    m_igdb_critic.set_text("—");
    m_igdb_rushed.set_text("—");
    m_igdb_normal.set_text("—");
    m_igdb_complete.set_text("—");
    m_igdb_connect.set_visible(true);
    m_igdb_link.set_visible(false);
    return;
  }
  m_igdb_status.set_text(info.updated.empty() ? "Updated from IGDB"
                                              : "Updated from IGDB · " + info.updated);
  m_igdb_critic.set_text(info.critic_score > 0
                             ? std::to_string(info.critic_score) + " / 100"
                             : "—");
  m_igdb_rushed.set_text(info.rushed_hours > 0
                             ? std::to_string(info.rushed_hours) + " H"
                             : "—");
  m_igdb_normal.set_text(info.normal_hours > 0
                             ? std::to_string(info.normal_hours) + " H"
                             : "—");
  m_igdb_complete.set_text(info.complete_hours > 0
                               ? std::to_string(info.complete_hours) + " H"
                               : "—");
  m_igdb_connect.set_visible(false);
  if (!info.url.empty()) {
    m_igdb_link.set_uri(info.url);
    m_igdb_link.set_visible(true);
  }
}

void DetailsView::set_achievements_loading() {
  m_achievement_status.set_text("Loading Steam achievements…");
  m_achievement_count.set_text("");
  m_achievement_percent.set_text("");
  clear_achievements();
}

void DetailsView::set_achievements(const SteamAchievementsResult& result) {
  clear_achievements();
  if (!result.available) {
    m_achievement_status.set_text(result.error.empty()
                                      ? "Steam achievements are unavailable."
                                      : result.error);
    return;
  }
  int unlocked = 0;
  for (const SteamAchievement& achievement : result.achievements) {
    if (achievement.unlocked) {
      ++unlocked;
    }
  }
  const int total = static_cast<int>(result.achievements.size());
  const double fraction = total > 0 ? static_cast<double>(unlocked) / total : 0.0;
  m_achievement_progress.set_fraction(fraction);
  m_achievement_percent.set_text(std::to_string(static_cast<int>(fraction * 100.0)) + "%");
  m_achievement_count.set_text(std::to_string(unlocked) + " / " + std::to_string(total));
  m_achievements_value.set_text(std::to_string(unlocked) + " / " + std::to_string(total));
  m_completion_value.set_text(std::to_string(static_cast<int>(fraction * 100.0)) + "%");
  m_achievement_status.set_text(total == 0 ? "No achievements for this game."
                                            : "Updated from your public Steam profile.");

  int row = 0;
  for (const SteamAchievement& achievement : result.achievements) {
    auto* item = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    item->add_css_class("achievement-row");
    if (!achievement.icon_path.empty() &&
        std::filesystem::is_regular_file(achievement.icon_path)) {
      auto* image = Gtk::make_managed<Gtk::Image>();
      image->set(achievement.icon_path);
      image->set_pixel_size(32);
      image->set_size_request(32, 32);
      item->append(*image);
    } else {
      auto* icon = Gtk::make_managed<Gtk::Label>(achievement.unlocked ? "✓" : "🔒");
      icon->set_size_request(28, -1);
      icon->set_xalign(0.5F);
      icon->add_css_class("achievement-icon");
      if (achievement.unlocked) {
        icon->add_css_class("achievement-unlocked");
      }
      item->append(*icon);
    }
    auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    text->set_hexpand(true);
    auto* name = Gtk::make_managed<Gtk::Label>(achievement.name);
    name->set_xalign(0.0F);
    name->set_ellipsize(Pango::EllipsizeMode::END);
    name->add_css_class("achievement-name");
    text->append(*name);
    auto* description = Gtk::make_managed<Gtk::Label>(achievement.description);
    description->set_xalign(0.0F);
    description->set_wrap(true);
    description->set_lines(2);
    description->set_ellipsize(Pango::EllipsizeMode::END);
    description->add_css_class("achievement-desc");
    text->append(*description);
    auto* status = Gtk::make_managed<Gtk::Label>(
        achievement.unlocked
            ? "UNLOCKED " + format_last_played(achievement.unlock_time) + " · STEAM"
            : "LOCKED · STEAM");
    status->set_xalign(0.0F);
    status->add_css_class("achievement-status");
    text->append(*status);
    item->append(*text);
    m_achievement_grid.attach(*item, row % 2, row / 2, 1, 1);
    ++row;
  }
}

void DetailsView::set_status_message(const std::string& text) {
  m_achievement_status.set_text(text);
}

void DetailsView::clear_achievements() {
  while (Gtk::Widget* child = m_achievement_grid.get_first_child()) {
    m_achievement_grid.remove(*child);
  }
}

} // namespace gameslauncher
