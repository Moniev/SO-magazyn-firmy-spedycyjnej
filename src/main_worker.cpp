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

Worker *global_worker_ptr = nullptr;

void signalHandler(int signum) {
  if (global_worker_ptr) {
    spdlog::warn("\n[main] SIGINT received (Signal {}). Finishing shift...",
                 signum);
    global_worker_ptr->stop();
  }
}

int main(int argc, char *argv[]) {
  int worker_id = (argc > 1) ? std::atoi(argv[1]) : getpid() % 1000;

  std::string logger_name = "worker-" + std::to_string(worker_id);
  Config::get().setupLogger(logger_name);

  spdlog::info("[main] Worker process starting with ID: {}", worker_id);

  Manager manager(false);

  if (!manager.session_store->login("Worker_" + std::to_string(worker_id),
                                    UserRole::Operator, 0, 10)) {
    spdlog::error("[main] Failed to login to SessionManager.");
    return 1;
  }

  Worker worker(&manager, worker_id);
  global_worker_ptr = &worker;

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  worker.run();

  spdlog::info("[main] Signing out...");
  manager.session_store->logout();

  return 0;
}
