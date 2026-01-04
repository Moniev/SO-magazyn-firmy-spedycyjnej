#include "../include/Manager.h"

int main() {
  Manager manager(true);

  spdlog::info("[ipc manager] Warehouse System Initialized.");

  while (manager.getState()->running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  spdlog::warn("[ipc manager] Shutdown signal received. Cleaning up.");
  return 0;
}
