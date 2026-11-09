#include "ui/epic_login.h"

#include "core/epic.h"

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

namespace gameslauncher {
namespace {

std::string json_string(JsonObject* object, const char* member) {
  if (object == nullptr || !json_object_has_member(object, member)) {
    return {};
  }
  JsonNode* node = const_cast<JsonNode*>(json_object_get_member(object, member));
  if (node == nullptr || !JSON_NODE_HOLDS_VALUE(node) ||
      json_node_get_value_type(node) != G_TYPE_STRING) {
    return {};
  }
  const char* value = json_node_get_string(node);
  return value != nullptr ? value : "";
}

std::string authorization_code(const std::string& text) {
  JsonParser* parser = json_parser_new();
  if (json_parser_load_from_data(parser, text.c_str(), static_cast<gssize>(text.size()),
                                 nullptr)) {
    JsonNode* root = json_parser_get_root(parser);
    JsonObject* object =
        (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
    const std::string code = json_string(object, "authorizationCode");
    g_object_unref(parser);
    if (!code.empty()) {
      return code;
    }
  } else {
    g_object_unref(parser);
  }
  const std::string marker = "\"authorizationCode\":\"";
  const std::size_t position = text.find(marker);
  if (position == std::string::npos) {
    return {};
  }
  const std::size_t first = position + marker.size();
  const std::size_t last = text.find('"', first);
  if (last == std::string::npos) {
    return {};
  }
  return text.substr(first, last - first);
}

} // namespace

EpicLoginDialog::EpicLoginDialog() {
  set_title("Sign in to Epic Games");
  set_default_size(480, 760);
  set_modal(true);
  m_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
  gtk_widget_set_hexpand(GTK_WIDGET(m_view), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(m_view), TRUE);
  gtk_window_set_child(GTK_WINDOW(gobj()), GTK_WIDGET(m_view));
  g_signal_connect(m_view, "load-changed", G_CALLBACK(on_load_changed), this);
  webkit_web_view_load_uri(m_view, EpicClient::login_url().c_str());
}

void EpicLoginDialog::on_load_changed(WebKitWebView* view, WebKitLoadEvent event,
                                      gpointer data) {
  auto* self = static_cast<EpicLoginDialog*>(data);
  if (self == nullptr || view == nullptr || event != WEBKIT_LOAD_FINISHED ||
      self->m_handled) {
    return;
  }
  const char* uri = webkit_web_view_get_uri(view);
  if (uri == nullptr ||
      !g_str_has_prefix(uri, "https://www.epicgames.com/id/api/redirect")) {
    return;
  }
  epic_log("login: redirect page reached");
  self->m_handled = true;
  const char* script =
      "(document.body ? (document.body.innerText || document.body.textContent) : '')"
      " || document.documentElement.innerText || ''";
  webkit_web_view_evaluate_javascript(view, script, -1, nullptr, nullptr, nullptr,
                                      on_javascript, self);
}

void EpicLoginDialog::on_javascript(GObject* source, GAsyncResult* result, gpointer data) {
  auto* self = static_cast<EpicLoginDialog*>(data);
  auto* view = WEBKIT_WEB_VIEW(source);
  GError* error = nullptr;
  JSCValue* value = webkit_web_view_evaluate_javascript_finish(view, result, &error);
  if (value == nullptr) {
    self->fail(error != nullptr ? error->message : "Epic sign-in did not return a code.");
    if (error != nullptr) {
      g_error_free(error);
    }
    return;
  }
  gchar* text = jsc_value_to_string(value);
  const std::string body = text != nullptr ? text : "";
  g_free(text);
  g_object_unref(value);
  self->handle_redirect(body);
}

void EpicLoginDialog::handle_redirect(const std::string& text) {
  const std::string code = authorization_code(text);
  epic_log(code.empty() ? "login: no authorizationCode in redirect body"
                        : "login: authorizationCode found");
  if (code.empty()) {
    fail("Epic did not return an authorization code. Try again.");
    return;
  }
  EpicSession session;
  std::string error;
  if (!EpicClient::exchange_code(code, &session, &error)) {
    fail(error.empty() ? "Epic sign-in failed." : error);
    return;
  }
  // Do not leave Epic login cookies or storage behind; the session is kept in
  // the desktop Secret Service instead.
  if (m_view != nullptr) {
    WebKitNetworkSession* network = webkit_web_view_get_network_session(m_view);
    WebKitWebsiteDataManager* manager =
        webkit_network_session_get_website_data_manager(network);
    webkit_website_data_manager_clear(manager, WEBKIT_WEBSITE_DATA_ALL, 0, nullptr, nullptr,
                                      nullptr);
  }
  m_signed_in.emit(session);
  close();
}

void EpicLoginDialog::fail(const std::string& message) {
  m_failed.emit(message);
  close();
}

} // namespace gameslauncher
