#pragma once

#include "core/ubisoft.h"

#include <gtkmm/window.h>
#include <sigc++/signal.h>
#include <webkit/webkit.h>

#include <string>

namespace gameslauncher {

// Embedded Ubisoft Connect login. The login happens on Ubisoft's page; LGL
// reads only the local session values after the redirect.
class UbisoftLoginDialog : public Gtk::Window {
public:
  UbisoftLoginDialog();

  sigc::signal<void(const UbisoftSession&)>& signal_signed_in() { return m_signed_in; }
  sigc::signal<void(const std::string&)>& signal_failed() { return m_failed; }

private:
  static void on_load_changed(WebKitWebView* view, WebKitLoadEvent event, gpointer data);
  static void on_javascript(GObject* source, GAsyncResult* result, gpointer data);
  void handle_storage(const std::string& text);
  void fail(const std::string& message);

  WebKitWebView* m_view = nullptr;
  bool m_handled = false;
  sigc::signal<void(const UbisoftSession&)> m_signed_in;
  sigc::signal<void(const std::string&)> m_failed;
};

} // namespace gameslauncher
