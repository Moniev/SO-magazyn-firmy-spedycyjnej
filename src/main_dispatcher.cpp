/**
 * @file dispatcher_main.cpp
 * @brief Dispatcher consumer process.
 * Uses existing SessionManager with RAII safety wrapper.
 */

#include "../include/Config.h"
#include "../include/Manager.h"

class DispatcherSession {
  Manager &m;

public:
  DispatcherSession(Manager &manager) : m(manager) {
    if (!m.session_store->login("System-Dispatcher", UserRole::Operator, 0,
                                1)) {
      throw std::runtime_error(
          "Critical: Could not log in to Warehouse System.");
    }
    spdlog::info("[dispatcher] Session authenticated successfully.");
  }

  ~DispatcherSession() {
    m.session_store->logout();
    spdlog::info("[dispatcher] Emergency/Standard Logout executed.");
  }
};

int main() {
  try {
    Config::get().setupLogger("system-dispatcher");

    Manager manager(false);
    DispatcherSession session(manager);

    spdlog::info("[dispatcher] Ready to route packages. Entering main loop.");

    manager.dispatcher->run();

  } catch (const std::exception &e) {
    spdlog::critical("[dispatcher] Process terminated by exception: {}",
                     e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
