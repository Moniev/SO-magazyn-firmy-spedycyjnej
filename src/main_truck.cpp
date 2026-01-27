/**
 * @file express_main.cpp
 * @brief High-priority worker reacting to SIGNAL_EXPRESS_LOAD.
 * * Unlike other workers, this process blocks on the Message Queue. When a
 * VIP signal is intercepted, it executes a direct-to-truck load, bypassing
 * the standard conveyor belt sequence.
 */
#include "../include/Config.h"
#include "../include/Manager.h"
#include <atomic>
#include <csignal>

std::atomic<bool> truck_stop{false};

void signalHandler(int signum) {
  spdlog::warn("[truck] Signal ({}) received. Driver finishing the shift...",
               signum);
  truck_stop.store(true);
}

struct TruckSessionGuard {
  Manager &m;
  TruckSessionGuard(Manager &manager) : m(manager) {
    if (!m.session_store->login("System-TruckPool", UserRole::Operator, 0, 5)) {
      throw std::runtime_error(
          "TruckPool login failed. All docking bays occupied?");
    }
    spdlog::info("[truck] Driver logged in. Docking permission granted.");
  }
  ~TruckSessionGuard() {
    m.session_store->logout();
    spdlog::info("[truck] Driver logged out. Bay cleared.");
  }
};

int main() {
  try {
    Config::get().setupLogger("system-truck");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Manager manager(false);
    TruckSessionGuard session(manager);

    spdlog::info("[truck] Truck process online. Heading to the dock...");

    manager.truck->run();

  } catch (const std::exception &e) {
    spdlog::critical("[truck] Critical Driver Error: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("[truck] Process finished cleanly. Iron Within.");
  return EXIT_SUCCESS;
}
