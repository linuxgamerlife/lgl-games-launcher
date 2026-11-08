#include "ui/themewatcher.h"

#include <adwaita.h>
#include <glib.h>
#include <gdkmm/display.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/stylecontext.h>

#include <string>

namespace gameslauncher {
namespace {

constexpr const char* kAppCss = "/com/linuxgamerlife/gameslauncher/style.css";

std::string user_gtk_dir() {
  return Glib::build_filename(Glib::get_user_config_dir(), "gtk-4.0");
}

} // namespace

ThemeWatcher::ThemeWatcher() = default;

ThemeWatcher::~ThemeWatcher() = default;

void ThemeWatcher::install() {
  adw_init();

  auto display = Gdk::Display::get_default();
  if (display == nullptr) {
    return;
  }

  auto app_provider = Gtk::CssProvider::create();
  try {
    app_provider->load_from_resource(kAppCss);
  } catch (const Glib::Error& error) {
    g_warning("Could not load application stylesheet: %s", error.what());
  }
  Gtk::StyleContext::add_provider_for_display(display, app_provider,
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  m_user_provider = Gtk::CssProvider::create();
  reload_user_css();
  Gtk::StyleContext::add_provider_for_display(display, m_user_provider,
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);

  try {
    const auto directory = Gio::File::create_for_path(user_gtk_dir());
    m_monitor = directory->monitor_directory(Gio::FileMonitorFlags::WATCH_MOVES);
    m_monitor->signal_changed().connect(
        [this](const Glib::RefPtr<Gio::File>&, const Glib::RefPtr<Gio::File>&,
               Gio::FileMonitor::Event) { schedule_reload(); });
  } catch (const Glib::Error&) {
    // A missing ~/.config/gtk-4.0 is not an error; GTK has defaults.
  }
}

void ThemeWatcher::schedule_reload() {
  m_timeout.disconnect();
  m_timeout = Glib::signal_timeout().connect(
      [this]() {
        reload_user_css();
        m_changed.emit();
        return false;
      },
      200);
}

void ThemeWatcher::reload_user_css() {
  if (!m_user_provider) {
    return;
  }
  const std::string path = Glib::build_filename(user_gtk_dir(), "gtk.css");
  if (!g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) {
    m_changed.emit();
    return;
  }
  try {
    m_user_provider->load_from_path(path);
  } catch (const Glib::Error&) {
    // User gtk.css commonly imports files that do not exist. GTK ignores
    // parse errors here; we do the same rather than interrupting the user.
  }
  m_changed.emit();
}

} // namespace gameslauncher
