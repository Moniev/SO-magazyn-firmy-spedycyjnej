/**
 * @file main_terminal.cpp
 * @brief Interactive Operator Console for the Warehouse IPC System.
 * * This process acts as the human-machine interface (HMI). It connects to the
 * existing IPC resources (Shared Memory, Message Queues) initialized by the
 * Master process.
 * * Functions:
 * - Provides a CLI for sending signals (VIP, DEPART, STOP).
 * - Authenticates as an Admin/Operator via SessionManager.
 * - Allows real-time interaction with background workers without stopping the
 * simulation.
 */

#include "../include/Manager.h"
#include "../include/terminal/TerminalManager.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <csignal>
#include <unistd.h>

std::atomic<bool> keep_running{true};

void signalHandler(int signal) {
  if (signal == SIGINT) {
    const char *msg = "\n[terminal] Shutdown signal received. Cleaning up...\n";
    write(STDOUT_FILENO, msg, 51);
    keep_running.store(false);
  }
}

struct AdminSessionGuard {
  Manager &m;
  AdminSessionGuard(Manager &manager) : m(manager) {
    if (!m.session_store->login(
            "AdminConsole", UserRole::Operator | UserRole::SysAdmin, 1, 1)) {
      throw std::runtime_error(
          "Authentication failed: Slot occupied or Master system offline.");
    }
    spdlog::info("[terminal] Admin access granted. Command link established.");
  }
  ~AdminSessionGuard() {
    m.session_store->logout();
    spdlog::info("[terminal] Admin session purged from SHM.");
  }
};

int main() {
  try {
    Manager manager(false);

    std::signal(SIGINT, signalHandler);

    AdminSessionGuard session(manager);
    TerminalManager terminal(&manager);
    spdlog::info(
        "[terminal] Console ready. Type 'help' for commands. Ctrl+C to exit.");

    while (keep_running.load() && manager.getState()->running) {
      terminal.runOnce();

      if (!keep_running.load())
        break;
    }

  } catch (const std::exception &e) {
    spdlog::critical("[terminal] System error: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("[terminal] Terminal process finished cleanly.");
  return EXIT_SUCCESS;
}
