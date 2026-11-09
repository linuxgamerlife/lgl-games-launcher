#pragma once

#include "core/epic.h"

#include <gtkmm/window.h>
#include <sigc++/signal.h>
#include <webkit/webkit.h>

#include <string>

namespace gameslauncher {

// Embedded Epic login window. It loads Epic's own login page, captures the
// authorization code from the redirect, and exchanges it for a session. The
// account password never passes through this application.
class EpicLoginDialog : public Gtk::Window {
public:
  EpicLoginDialog();

  sigc::signal<void(const EpicSession&)>& signal_signed_in() { return m_signed_in; }
  sigc::signal<void(const std::string&)>& signal_failed() { return m_failed; }

private:
  static void on_load_changed(WebKitWebView* view, WebKitLoadEvent event, gpointer data);
  static void on_javascript(GObject* source, GAsyncResult* result, gpointer data);
  void handle_redirect(const std::string& text);
  void fail(const std::string& message);

  WebKitWebView* m_view = nullptr;
  bool m_handled = false;
  sigc::signal<void(const EpicSession&)> m_signed_in;
  sigc::signal<void(const std::string&)> m_failed;
};

} // namespace gameslauncher
