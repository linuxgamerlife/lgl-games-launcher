#include "ui/ubisoft_login.h"

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

} // namespace

UbisoftLoginDialog::UbisoftLoginDialog() {
  set_title("Sign in to Ubisoft Connect");
  set_icon_name("lgl-games-launcher");
  set_default_size(480, 760);
  set_modal(true);
  m_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
  gtk_widget_set_hexpand(GTK_WIDGET(m_view), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(m_view), TRUE);
  gtk_window_set_child(GTK_WINDOW(gobj()), GTK_WIDGET(m_view));
  g_signal_connect(m_view, "load-changed", G_CALLBACK(on_load_changed), this);
  webkit_web_view_load_uri(m_view, UbisoftClient::login_url().c_str());
}

void UbisoftLoginDialog::on_load_changed(WebKitWebView* view, WebKitLoadEvent event,
                                         gpointer data) {
  auto* self = static_cast<UbisoftLoginDialog*>(data);
  if (self == nullptr || view == nullptr || event != WEBKIT_LOAD_FINISHED ||
      self->m_handled) {
    return;
  }
  const char* uri = webkit_web_view_get_uri(view);
  if (uri == nullptr || !g_str_has_prefix(uri, "https://connect.ubisoft.com/")) {
    return;
  }
  if (g_str_has_prefix(uri, "https://connect.ubisoft.com/ready")) {
    webkit_web_view_evaluate_javascript(
        view, "window.location.replace('https://connect.ubisoft.com/change_domain/')", -1,
        nullptr, nullptr, nullptr, nullptr, nullptr);
    return;
  }
  // Only read the session values on Ubisoft's explicit post-login redirect.
  // The initial login page can contain stale storage without a ticket.
  if (!g_str_has_prefix(uri, "https://connect.ubisoft.com/change_domain")) {
    return;
  }
  const char* script =
      "(() => {"
      "  const login = localStorage.getItem('PRODloginData');"
      "  if (!login) return '';"
      "  return JSON.stringify({"
      "    login: login,"
      "    remember: localStorage.getItem('PRODrememberMe') || '',"
      "    profile: localStorage.getItem('PRODlastProfile') || ''"
      "  });"
      "})()";
  webkit_web_view_evaluate_javascript(view, script, -1, nullptr, nullptr, nullptr,
                                      on_javascript, self);
}

void UbisoftLoginDialog::on_javascript(GObject* source, GAsyncResult* result, gpointer data) {
  auto* self = static_cast<UbisoftLoginDialog*>(data);
  auto* view = WEBKIT_WEB_VIEW(source);
  GError* error = nullptr;
  JSCValue* value = webkit_web_view_evaluate_javascript_finish(view, result, &error);
  if (value == nullptr) {
    if (error != nullptr) {
      g_error_free(error);
    }
    return;
  }
  gchar* text = jsc_value_to_string(value);
  const std::string body = text != nullptr ? text : "";
  g_free(text);
  g_object_unref(value);
  if (body.empty() || body == "''") {
    return;
  }
  self->handle_storage(body);
}

void UbisoftLoginDialog::handle_storage(const std::string& text) {
  JsonParser* parser = json_parser_new();
  if (!json_parser_load_from_data(parser, text.c_str(), static_cast<gssize>(text.size()),
                                  nullptr)) {
    g_object_unref(parser);
    return;
  }
  JsonNode* root = json_parser_get_root(parser);
  JsonObject* object =
      (root != nullptr && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root) : nullptr;
  const std::string login = json_string(object, "login");
  const std::string remember = json_string(object, "remember");
  const std::string profile = json_string(object, "profile");
  g_object_unref(parser);
  if (login.empty()) {
    return;
  }
  m_handled = true;
  ubisoft_log("login: storage found");
  UbisoftSession session;
  std::string error;
  if (!UbisoftClient::authorise_with_storage(login, remember, profile, &session, &error)) {
    fail(error.empty() ? "Ubisoft sign-in failed." : error);
    return;
  }
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

void UbisoftLoginDialog::fail(const std::string& message) {
  ubisoft_log("login: failed " + message);
  m_failed.emit(message);
  close();
}

} // namespace gameslauncher
