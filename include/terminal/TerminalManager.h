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
#include <string>

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
  void run() {
    if (manager->session_store->getSessionIndex() == -1) {
      std::cerr << "Terminal error: Not logged in via SessionManager!\n";
      return;
    }

    spdlog::info("[cli] Running operator interface");
    printHeader();

    std::string commandInput;
    printPrompt();

    while (active && std::cin >> commandInput) {
      std::transform(commandInput.begin(), commandInput.end(),
                     commandInput.begin(), ::tolower);

      UserRole myRole = manager->session_store->getCurrentRole();
      CliCommand cmd = CommandResolver::resolve(commandInput);

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

      case CliCommand::Unknown:
      default:
        std::cout << "  └─ Unknown command.\n";
        break;
      }

      if (active)
        printPrompt();
    }
  }
};
