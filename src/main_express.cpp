#include "../include/Config.h"
#include "../include/Manager.h"
#include <atomic>
#include <csignal>
#include <iostream>

std::atomic<bool> stop_requested{false};

void signalHandler(int signum) { stop_requested.store(true); }

struct P4SessionGuard {
  Manager &m;
  P4SessionGuard(Manager &manager) : m(manager) {
    if (!m.session_store->login("System-Express", UserRole::Operator, 0, 1)) {
      throw std::runtime_error(
          "P4 Login failed. Is Express process already running?");
    }
    spdlog::info("[express-proc] P4 Worker logged in and ready.");
  }

  ~P4SessionGuard() {
    m.session_store->logout();
    spdlog::info("[express-proc] P4 Worker logged out.");
  }
};

int main() {
  try {
    Config::get().setupLogger("system-express");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Manager manager(false);
    P4SessionGuard session(manager);

    spdlog::info("[express-proc] P4 Standing by. Waiting for Signal 2 (Express "
                 "Load)...");

    pid_t my_pid = getpid();

    while (!stop_requested.load() && manager.getState()->running) {
      SignalType sig = manager.receiveSignalBlocking(my_pid);

      if (sig == SIGNAL_EXPRESS_LOAD) {
        spdlog::info(
            "[express-proc] Signal 2 Received! Starting batch delivery.");

        manager.express->deliverExpressBatch();

        spdlog::info(
            "[express-proc] Batch delivery finished. Returning to standby.");
      } else if (sig == SIGNAL_END_WORK) {
        spdlog::info(
            "[express-proc] Signal 3 (End Work) received. Shutting down.");
        break;
      } else if (sig == SIGNAL_DEPARTURE) {
      }
    }

  } catch (const std::exception &e) {
    spdlog::critical("[express-proc] Critical Error: {}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
