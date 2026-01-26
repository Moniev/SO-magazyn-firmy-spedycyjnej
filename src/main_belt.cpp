/**
 * @file belt_main.cpp
 * @brief Producer process that generates and pushes packages onto the belt.
 * * This worker connects to existing IPC resources (`owner=false`) and logs in
 * via the SessionManager. It operates in a loop, creating new packages with
 * randomized weights and pushing them into the circular buffer.
 */

#include "../include/Config.h"
#include "../include/Manager.h"
#include <chrono>
#include <thread>

int main() {
  Config::get().setupLogger("system-belt");

  spdlog::info("[belt-proc] Process started. Initializing Belt Subsystem...");

  Manager manager(false);

  if (!manager.session_store->login("System-Belt", UserRole::Operator, 0, 1)) {
    spdlog::error("[belt-proc] Login failed. Exiting.");
    return 1;
  }

  spdlog::info(
      "[belt-proc] Connected. Waiting for workers to start production...");

  while (manager.getState()->running) {
    static int log_counter = 0;
    if (++log_counter >= 5) {
      int count = manager.belt->getCount();
      int workers = manager.belt->getWorkerCount();
      spdlog::info("[belt-proc] Status: {} items on belt, {} active workers.",
                   count, workers);
      log_counter = 0;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  spdlog::info("[belt-proc] System shutdown signal received.");
  manager.session_store->logout();
  return 0;
}
