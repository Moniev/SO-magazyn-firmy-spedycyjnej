/**
 * @file TerminalAction.h
 * @brief Implementation of CLI command logic and security checks.
 */

#include "../Manager.h"
#include "../Shared.h"
#include <iostream>
#include <string>

/**
 * @class TerminalActions
 * @brief Static container for command execution logic.
 * * Each method in this class corresponds to a specific user intent.
 * The methods verify if the current user session possesses the required
 * `UserRole` bitflags before dispatching signals via the Manager.
 */
class TerminalActions {
public:
  /**
   * @brief Handles the 'vip' command.
   * * **Permission**: Operator or SysAdmin.
   * * **Action**: Sends SIGNAL_EXPRESS_LOAD to the message queue.
   */
  static void handleVip(Manager *manager, UserRole role) {
    if (hasFlag(role, UserRole::Operator) ||
        hasFlag(role, UserRole::SysAdmin)) {
      manager->sendSignal(SIGNAL_EXPRESS_LOAD);
      std::cout << "  └─ \033[36mVIP Request Sent.\033[0m\n";
    } else {
      printAccessDenied("Operator");
    }
  }

  /**
   * @brief Handles the 'depart' command.
   * * **Permission**: Operator or SysAdmin.
   * * **Action**: Sends SIGNAL_DEPARTURE to the message queue.
   */
  static void handleDepart(Manager *manager, UserRole role) {
    if (hasFlag(role, UserRole::Operator) ||
        hasFlag(role, UserRole::SysAdmin)) {
      manager->sendSignal(SIGNAL_DEPARTURE);
      std::cout << "  └─ \033[33mDeparture Signal Sent.\033[0m\n";
    } else {
      printAccessDenied("Operator");
    }
  }

  /**
   * @brief Handles the 'stop' command.
   * * **Permission**: SysAdmin ONLY.
   * * **Action**: Sends SIGNAL_END_WORK (Emergency Stop) and terminates the CLI
   * loop.
   * @param activeRef Reference to the main loop flag, set to false on success.
   */
  static void handleStop(Manager *manager, UserRole role, bool &activeRef) {
    if (hasFlag(role, UserRole::SysAdmin)) {
      spdlog::critical("[cli] EMERGENCY STOP SENT");
      manager->sendSignal(SIGNAL_END_WORK);
      std::cout << "  └─ \033[31mSYSTEM HALT COMMANDED.\033[0m\n";
      activeRef = false;
    } else {
      std::cout << "  └─ \033[31mACCESS DENIED.\033[0m Only SysAdmin can stop "
                   "the system.\n";
    }
  }

private:
  /** @brief Helper to print standardized error messages. */
  static void printAccessDenied(const std::string &requiredRole) {
    std::cout << "  └─ \033[31mPermission Denied.\033[0m Need " << requiredRole
              << " role.\n";
  }
};
