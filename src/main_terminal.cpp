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

int main() {
  Manager manager(false);

  if (!manager.session_store->login(
          "AdminConsole", UserRole::Operator | UserRole::SysAdmin, 1, 1)) {
    spdlog::critical("[terminal manager] failed to run terminal manager");

    return 1;
  }

  TerminalManager terminal(&manager);
  spdlog::info("[terminal manager] VIP Handler ready.");

  terminal.run();

  manager.session_store->logout();
  return 0;
}
