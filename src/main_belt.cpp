/**
 * @file belt_main.cpp
 * @brief Producer process that generates and pushes packages onto the belt.
 * * This worker connects to existing IPC resources (`owner=false`) and logs in
 * via the SessionManager. It operates in a loop, creating new packages with
 * randomized weights and pushing them into the circular buffer.
 */
#include "../include/Config.h"
#include "../include/Manager.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

std::atomic<bool> stop_flag{false};

void signalHandler(int signum) {
  spdlog::warn("[belt-proc] Signal ({}) received. Safe shutdown initiated.",
               signum);
  stop_flag.store(true);
}

struct BeltSessionGuard {
  Manager &m;
  BeltSessionGuard(Manager &manager) : m(manager) {
    if (!m.session_store->login("System-Belt", UserRole::Operator, 0, 1)) {
      throw std::runtime_error("Belt Subsystem Login failed. Registry full?");
    }
    spdlog::info("[belt-proc] Session authenticated. Monitoring active.");
  }
  ~BeltSessionGuard() {
    m.session_store->logout();
    spdlog::info("[belt-proc] Belt Session cleared from SHM.");
  }
};

int main() {
  try {
    Config::get().setupLogger("system-belt");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Manager manager(false);
    BeltSessionGuard session(manager);

    spdlog::info("[belt-proc] Connected to IPC. Observing buffer metrics...");

    int log_counter = 0;

    while (!stop_flag.load() && manager.getState()->running) {

      if (++log_counter >= 5) {
        int count = manager.belt->getCount();
        int workers = manager.belt->getWorkerCount();

        spdlog::info(
            "[belt-proc] Status: {:02d} items on belt | {:02d} active workers.",
            count, workers);
        log_counter = 0;
      }

      for (int i = 0; i < 10; ++i) {
        if (stop_flag.load() || !manager.getState()->running)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    spdlog::info("[belt-proc] Monitoring finished. Relinquishing control.");

  } catch (const std::exception &e) {
    spdlog::critical("[belt-proc] Fatal Exception: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
