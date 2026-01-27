#include "../include/Config.h"
#include "../include/Manager.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <string>

std::atomic<bool> truck_stop{false};

void signalHandler(int signum) {
  spdlog::warn("[truck] Signal ({}) received. Driver finishing the shift...",
               signum);
  truck_stop.store(true);
}

struct TruckSessionGuard {
  Manager &m;

  TruckSessionGuard(Manager &manager, const std::string &username)
      : m(manager) {
    if (!m.session_store->login(username, UserRole::Operator, 0, 1)) {
      throw std::runtime_error(
          "Login failed for user '" + username +
          "'. Is the session table full or user already logged in?");
    }
    spdlog::info(
        "[truck] Driver logged in as '{}'. Docking permission granted.",
        username);
  }

  ~TruckSessionGuard() {
    m.session_store->logout();
    spdlog::info("[truck] Driver logged out. Bay cleared.");
  }
};

int main(int argc, char *argv[]) {
  try {
    int truck_id = (argc > 1) ? std::atoi(argv[1]) : 1;
    std::string id_str = std::to_string(truck_id);

    Config::get().setupLogger("truck-" + id_str);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Manager manager(false);

    std::string unique_username = "Truck_" + id_str;
    TruckSessionGuard session(manager, unique_username);

    spdlog::info("[truck] Truck process #{} online. Heading to the dock...",
                 truck_id);
    manager.truck->run();

  } catch (const std::exception &e) {
    spdlog::critical("[truck] Critical Driver Error: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("[truck] Process finished cleanly. Iron Within.");
  return EXIT_SUCCESS;
}
