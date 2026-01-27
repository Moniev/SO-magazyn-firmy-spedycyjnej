/**
 * @file TerminalManager.h
 * @brief The main interactive loop for the Operator Console.
 */

#pragma once

#include "../Shared.h"
#include "CommandResolver.h"
#include "TerminalAction.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <string>

extern std::atomic<bool> keep_running;

/**
 * @class TerminalManager
 * @brief Manages the standard input/output (stdio) loop for user interaction.
 * * It retrieves the current user's session context (Username, Role, OrgID)
 * to render the header and prompt. It reads input, resolves commands, and
 * triggers the appropriate TerminalAction.
 */
class TerminalManager {
private:
  Manager *manager; /**< Pointer to the central IPC Manager. */
  bool active;      /**< Loop control flag. */
  bool header_printed = false;

  /**
   * @brief Retrieves the active session for the current process.
   * @return Pointer to UserSession or nullptr if not logged in.
   */
  UserSession *getCurrentSession() {
    int idx = manager->session_store->getSessionIndex();
    if (idx >= 0 && idx < MAX_USERS_SESSIONS) {
      return &manager->getState()->users[idx];
    }
    return nullptr;
  }

  /** @brief Renders the dashboard ASCII header with session info. */
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
    std::cout << "║ Ctrl + C             ║ Exit from console             ║\n";
    std::cout << "║ exit / quit          ║ As above                      ║\n";
    std::cout << "╚══════════════════════╩═══════════════════════════════╝\n";
  }

  /** @brief Renders the prompt string based on privilege level (Green/Red). */
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
   * @brief Initializes the CLI manager.
   * @param mgr Pointer to the initialized IPC Manager instance.
   */
  TerminalManager(Manager *mgr) : manager(mgr), active(true) {}

  /**
   * @brief Starts the read-eval-print loop (REPL).
   * * Reads stdin line-by-line, converts to lowercase, resolves commands,
   * and delegates to TerminalActions.
   * * Terminates when 'exit' is typed or 'stop' (if admin) is executed.
   */
  void runOnce() {
    if (!header_printed) {
      printHeader();
      header_printed = true;
      printPrompt();
    }

    struct pollfd fds;
    fds.fd = STDIN_FILENO;
    fds.events = POLLIN;

    int ret = poll(&fds, 1, 100);

    if (ret > 0 && (fds.revents & POLLIN)) {
      std::string line;
      if (!std::getline(std::cin, line) || line == "exit" || line == "quit") {
        keep_running.store(false);
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
        // Stop ustawia active na false, co przerywa pętlę
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
      }

      if (keep_running.load()) {
        printPrompt();
      }
    }
  }
};
