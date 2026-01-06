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
#include <csignal>

Manager *global_manager = nullptr;

/**
 * @brief Signal handler to ensure session logout on Ctrl+C (SIGINT).
 */
void signalHandler(int signal) {
  if (signal == SIGINT && global_manager) {
    spdlog::warn("\n[terminal manager] Caught SIGINT! Cleaning up session...");
    global_manager->session_store->logout();
    exit(0);
  }
}

int main() {
  Manager manager(false);
  global_manager = &manager;

  std::signal(SIGINT, signalHandler);

  if (!manager.session_store->login(
          "AdminConsole", UserRole::Operator | UserRole::SysAdmin, 1, 1)) {
    spdlog::critical("[terminal manager] Authentication failed. User already "
                     "logged in or system down.");
    return 1;
  }

  TerminalManager terminal(&manager);
  spdlog::info("[terminal manager] VIP Handler ready. Press Ctrl+C to exit.");

  terminal.run();

  manager.session_store->logout();
  spdlog::info("[terminal manager] Console session closed.");

  return 0;
}
