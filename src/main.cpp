/**
 * @file main.cpp
 * @brief Master process responsible for IPC initialization and lifecycle
 * management.
 * * This process instantiates the Manager with the `owner=true` flag, which
 * triggers the creation of Shared Memory, Semaphores, and Message Queues.
 * It remains active to prevent the OS from potentially cleaning up resources
 * while workers are still running.
 */

#include "../include/Config.h"
#include "../include/Manager.h"
#include <filesystem>

int main() {
  if (!std::filesystem::exists("logs")) {
    std::filesystem::create_directory("logs");
  }

  Config::get().setupLogger("system-master");

  Manager manager(true);

  spdlog::info("[ipc manager] Warehouse System Initialized.");

  while (manager.getState()->running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  spdlog::warn("[ipc manager] Shutdown signal received. Cleaning up.");
  return 0;
}
