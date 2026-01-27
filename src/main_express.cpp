/**
 * @file dispatcher_main.cpp
 * @brief Consumer process that routes packages from the belt to the truck.
 * * Uses the Dispatcher component to block on the belt (waiting for items)
 * and then synchronize with the dock to perform the transfer.
 */
#include "../include/Config.h"
#include "../include/Express.h"
#include "../include/Manager.h"
#include <atomic>
#include <csignal>

std::atomic<bool> stop_flag{false};

void signalHandler(int signum) {
  spdlog::warn(
      "[express-proc] Signal ({}) received. Shutting down gracefully...",
      signum);
  stop_flag.store(true);
}

struct ExpressSessionGuard {
  Manager &m;
  ExpressSessionGuard(Manager &manager) : m(manager) {
    if (!m.session_store->login("System-Express", UserRole::Operator, 0, 1)) {
      throw std::runtime_error(
          "VIP Login failed - Unauthorized access to Express Service.");
    }
    spdlog::info("[express-proc] VIP Session authenticated.");
  }
  ~ExpressSessionGuard() {
    m.session_store->logout();
    spdlog::info("[express-proc] VIP Session closed safely.");
  }
};

int main() {
  try {
    Config::get().setupLogger("system-express");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Manager manager(false);

    ExpressSessionGuard session(manager);

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> sleep_dist(5000, 10000);

    spdlog::info(
        "[express-proc] VIP Service online. Awaiting priority packages...");

    while (!stop_flag.load() && manager.getState()->running) {

      int sleep_total_ms = sleep_dist(gen);
      int slept_ms = 0;

      while (slept_ms < sleep_total_ms && !stop_flag.load() &&
             manager.getState()->running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        slept_ms += 100;
      }

      if (!stop_flag.load() && manager.getState()->running) {
        manager.express->deliverVipPackage();
      }
    }

  } catch (const std::exception &e) {
    spdlog::critical("[express-proc] VIP System Failure: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
