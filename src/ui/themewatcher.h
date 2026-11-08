#pragma once

#include <giomm/filemonitor.h>
#include <gtkmm/cssprovider.h>
#include <sigc++/signal.h>

namespace gameslauncher {

// Loads the application stylesheet from the GResource, reloads the user's
// gtk.css when it changes, and emits after each re-theme.
class ThemeWatcher {
public:
  ThemeWatcher();
  ~ThemeWatcher();
  ThemeWatcher(const ThemeWatcher&) = delete;
  ThemeWatcher& operator=(const ThemeWatcher&) = delete;

  void install();

  [[nodiscard]] sigc::signal<void()>& signal_changed() { return m_changed; }

private:
  void reload_user_css();
  void schedule_reload();

  Glib::RefPtr<Gtk::CssProvider> m_user_provider;
  Glib::RefPtr<Gio::FileMonitor> m_monitor;
  sigc::connection m_timeout;
  sigc::signal<void()> m_changed;
};

} // namespace gameslauncher
