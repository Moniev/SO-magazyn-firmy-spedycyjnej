/**
 * @file worker_main.cpp
 * @brief Entry point for a Worker process.
 * * Usage: ./worker <ID>
 * * Connects to the system, registers on the belt, and starts generating
 * packages.
 */
#include "../include/Config.h"
#include "../include/Manager.h"
#include "../include/Worker.h"
#include <csignal>
#include <iostream>

std::atomic<bool> should_stop{false};
Worker *global_worker_ptr = nullptr;

void signalHandler(int signum) {
  spdlog::warn("[worker] Signal {} intercepted. Cooldown initiated...", signum);
  should_stop.store(true);
  if (global_worker_ptr) {
    global_worker_ptr->stop();
  }
}

struct WorkerSessionGuard {
  Manager &m;
  std::string name;
  WorkerSessionGuard(Manager &manager, const std::string &worker_name)
      : m(manager), name(worker_name) {
    if (!m.session_store->login(name, UserRole::Operator, 0, 10)) {
      throw std::runtime_error(
          "System Overload: Cannot register worker session.");
    }
    spdlog::info("[session] Worker '{}' logged in.", name);
  }
  ~WorkerSessionGuard() {
    m.session_store->logout();
    spdlog::info("[session] Worker '{}' logged out safely.", name);
  }
};

int main(int argc, char *argv[]) {
  try {
    int worker_id = (argc > 1) ? std::stoi(argv[1]) : getpid() % 1000;
    Config::get().setupLogger("worker-" + std::to_string(worker_id));
    Manager manager(false);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    WorkerSessionGuard session(manager, "Worker_" + std::to_string(worker_id));

    Worker worker(&manager, worker_id);
    global_worker_ptr = &worker;

    spdlog::info("[main] Worker {} starting shift.", worker_id);
    worker.run();

  } catch (const std::exception &e) {
    spdlog::critical("[main] Worker fatal error: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("[main] Clean exit. Shift finished.");
  return EXIT_SUCCESS;
}
