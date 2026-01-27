/**
 * @file TerminalManager.h
 * @brief Manages the interactive Command Line Interface (CLI) for the operator.
 *
 * This file defines the `TerminalManager` class, which serves as the User
 * Interface for the Warehouse Simulation. It handles user input, command
 * parsing, authorization checks, and visual rendering of the console menu.
 */
#pragma once

#include "../Shared.h"
#include "CommandResolver.h"
#include "TerminalAction.h"
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <string>
#include <unistd.h>

/**
 * @brief Global atomic flag to signal the application loop to continue or stop.
 * Defined externally (usually in main.cpp).
 */
extern std::atomic<bool> keep_running;

/**
 * @class TerminalManager
 * @brief Controls the operator's console session.
 *
 * The TerminalManager implements a non-blocking Read-Eval-Print Loop (REPL).
 * It allows authorized users (Operators/Admins) to interact with the running
 * simulation without halting the background processes.
 *
 * Key Responsibilities:
 * - **Session Context:** Retrieving the current user's identity and role.
 * - **Input Handling:** Reading stdin using `poll()` to avoid blocking the main
 * thread.
 * - **Command Dispatch:** Delegating resolved commands to `TerminalActions`.
 * - **UI Rendering:** Drawing the ASCII status header and command prompt.
 */
class TerminalManager {
private:
  /** @brief Pointer to the central Manager instance for system access. */
  Manager *manager;

  /** @brief Internal flag indicating if the terminal session is active. */
  bool active;

  /** @brief Flag to ensure the welcome header is printed only once per session.
   */
  bool header_printed = false;

  /**
   * @brief Retrieves the active user session associated with this process.
   *
   * Queries the `SessionManager` for the current process's index in the shared
   * session table.
   *
   * @return Pointer to the `UserSession` struct, or `nullptr` if not found.
   */
  UserSession *getCurrentSession() {
    int idx = manager->session_store->getSessionIndex();
    if (idx >= 0 && idx < MAX_USERS_SESSIONS) {
      return &manager->getState()->users[idx];
    }
    return nullptr;
  }

  /**
   * @brief Renders the ASCII dashboard header.
   *
   * Displays:
   * - System Title and Version.
   * - Current User, Organization ID, and Role Mask.
   * - Available Commands menu (dynamically filtered by role permissions).
   */
  void printHeader() {
    UserSession *s = getCurrentSession();
    std::string user = s ? s->username : "Unknown";
    int org = s ? s->orgId : -1;
    UserRole role = s ? s->role : UserRole::None;

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║             WAREHOUSE COMMAND CENTER v2.0            ║\n";
    std::cout << "╠══════════════════════════════════════════════════════╣\n";
    std::cout << "║ User: " << std::left << std::setw(15) << user
              << " OrgID: " << std::setw(6) << org
              << " RoleMask: " << std::setw(3) << (int)role << " ║\n";
    std::cout << "╠══════════════════════╦═══════════════════════════════╣\n";
    std::cout << "║ COMMAND              ║ ACTION                        ║\n";
    std::cout << "╠══════════════════════╬═══════════════════════════════╣\n";
    std::cout << "║ vip                  ║ Pass VIP package (Operator)   ║\n";
    std::cout << "║ depart               ║ Force TRUCK depart (Operator) ║\n";
    if (hasFlag(role, UserRole::SysAdmin)) {
      std::cout << "║ stop                 ║ \033[31mEMERGENCY STOP "
                   "(Admin)\033[0m        ║\n";
    }
    std::cout << "║ help                 ║ Print menu                    ║\n";
    std::cout << "║ exit / quit          ║ Exit console                  ║\n";
    std::cout << "╚══════════════════════╩═══════════════════════════════╝\n";
  }

  /**
   * @brief Prints the command line prompt.
   *
   * Color-codes the prompt based on privilege level:
   * - **Red:** Admin ("admin #")
   * - **Green:** Standard User/Operator ("user $")
   */
  void printPrompt() {
    UserSession *s = getCurrentSession();
    if (s && hasFlag(s->role, UserRole::SysAdmin)) {
      std::cout << "\033[1;31madmin\033[0m # " << std::flush;
    } else {
      std::cout << "\033[1;32muser\033[0m $ " << std::flush;
    }
  }

public:
  /**
   * @brief Constructs the Terminal Manager.
   * @param mgr Pointer to the main system Manager instance.
   */
  TerminalManager(Manager *mgr) : manager(mgr), active(true) {}

  /**
   * @brief Executes one iteration of the CLI loop.
   *
   * This method is designed to be called cyclically. It performs the following:
   * 1. **Header Check:** Prints the menu if it's the first run.
   * 2. **Input Polling:** Checks `stdin` for data using `poll()` with a 100ms
   * timeout. This prevents the terminal from blocking the main thread
   * indefinitely.
   * 3. **Command Processing:**
   * - Reads the line if input is available.
   * - Resolves the string to a `CliCommand` enum via `CommandResolver`.
   * - Executes the corresponding logic via `TerminalActions`.
   * 4. **State Management:** Updates `keep_running` or `active` flags based on
   * commands.
   */
  void runOnce() {
    if (!header_printed) {
      printHeader();
      header_printed = true;
      printPrompt();
    }

    bool input_ready = false;

    if (std::cin.rdbuf()->in_avail() > 0) {
      input_ready = true;
    } else {
      struct pollfd fds;
      fds.fd = STDIN_FILENO;
      fds.events = POLLIN;
      int ret = poll(&fds, 1, 100);
      if (ret > 0 && (fds.revents & POLLIN)) {
        input_ready = true;
      }
    }

    if (input_ready) {
      std::string line;
      if (!std::getline(std::cin, line) || line == "exit" || line == "quit") {
        keep_running.store(false);
        active = false;
        return;
      }

      if (line.empty()) {
        printPrompt();
        return;
      }

      std::transform(line.begin(), line.end(), line.begin(), ::tolower);

      CliCommand cmd = CommandResolver::resolve(line);
      UserRole myRole = manager->session_store->getCurrentRole();

      switch (cmd) {
      case CliCommand::Vip:
        TerminalActions::handleVip(manager, myRole);
        break;
      case CliCommand::Depart:
        TerminalActions::handleDepart(manager, myRole);
        break;
      case CliCommand::Stop:
        TerminalActions::handleStop(manager, myRole, active);
        break;
      case CliCommand::Help:
        printHeader();
        break;
      case CliCommand::Exit:
        active = false;
        break;
      default:
        std::cout << "  └─ Unknown command.\n";
        break;
      }

      if (!active) {
        keep_running.store(false);
      } else if (keep_running.load()) {
        printPrompt();
      }
    }
  }
};
