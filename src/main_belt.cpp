/**
 * @file belt_main.cpp
 * @brief Producer process that generates and pushes packages onto the belt.
 * * This worker connects to existing IPC resources (`owner=false`) and logs in
 * via the SessionManager. It operates in a loop, creating new packages with
 * randomized weights and pushing them into the circular buffer.
 */

#include "../include/Manager.h"
#include <chrono>
#include <thread>

int main() {
  Manager manager(false);

  if (!manager.session_store->login("System-Belt", UserRole::Operator, 0, 1)) {
    return 1;
  }

  spdlog::info("[belt] Started. Generating packages.");

  while (manager.getState()->running) {
    if (manager.session_store->trySpawnProcess()) {
      Package p;
      p.weight = 1.0 + (rand() % 100) / 10.0;

      manager.belt->push(p);

      manager.session_store->reportProcessFinished();
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  manager.session_store->logout();
  return 0;
}
