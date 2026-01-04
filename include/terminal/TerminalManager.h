#pragma once

#include "../Shared.h"
#include "CommandResolver.h"
#include "TerminalAction.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

class TerminalManager {
private:
  Manager *manager;
  bool active;

  UserSession *getCurrentSession() {
    int idx = manager->session_store->getSessionIndex();
    if (idx >= 0 && idx < MAX_USERS_SESSIONS) {
      return &manager->getState()->users[idx];
    }
    return nullptr;
  }

  void printHeader() {
    UserSession *s = getCurrentSession();
    std::string user = s ? s->username : "Unknown";
    int org = s ? s->orgId : -1;
    UserRole role = s ? s->role : UserRole::None;

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║            WAREHOUSE COMMAND CENTER v2.0             ║\n";
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
    std::cout << "║ exit / quit          ║ Exit from console             ║\n";
    std::cout << "╚══════════════════════╩═══════════════════════════════╝\n";
  }

  void printPrompt() {
    UserSession *s = getCurrentSession();
    if (s && hasFlag(s->role, UserRole::SysAdmin)) {
      std::cout << "\033[1;31madmin\033[0m # " << std::flush;
    } else {
      std::cout << "\033[1;32muser\033[0m $ " << std::flush;
    }
  }

public:
  TerminalManager(Manager *mgr) : manager(mgr), active(true) {}

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
