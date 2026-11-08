#include "ui/window.h"

#include <adwaita.h>
#include <glibmm/miscutils.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include <memory>

int main(int argc, char* argv[]) {
  adw_init();
  Glib::set_application_name("LGL Games Launcher");
  Gtk::Window::set_default_icon_name("lgl-games-launcher");

  auto application =
      Gtk::Application::create("com.linuxgamerlife.lgl-games-launcher");
  std::unique_ptr<gameslauncher::Window> window;

  application->signal_startup().connect([&] {
    application->add_action("quit", [&] {
      if (window) {
        window->set_visible(false);
      }
      application->quit();
    });
    application->set_accel_for_action("app.quit", "<Control>q");
  });

  application->signal_activate().connect([&] {
    if (!window) {
      window = std::make_unique<gameslauncher::Window>(application);
    }
    window->present();
  });

  const int status = application->run(argc, argv);
  window.reset();
  return status;
}
