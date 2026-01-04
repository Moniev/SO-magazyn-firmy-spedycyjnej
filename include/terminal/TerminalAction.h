#include "../Manager.h"
#include "../Shared.h"
#include <iostream>
#include <string>

class TerminalActions {
public:
  static void handleVip(Manager *manager, UserRole role) {
    if (hasFlag(role, UserRole::Operator) ||
        hasFlag(role, UserRole::SysAdmin)) {
      manager->sendSignal(SIGNAL_EXPRESS_LOAD);
      std::cout << "  └─ \033[36mVIP Request Sent.\033[0m\n";
    } else {
      printAccessDenied("Operator");
    }
  }

  static void handleDepart(Manager *manager, UserRole role) {
    if (hasFlag(role, UserRole::Operator) ||
        hasFlag(role, UserRole::SysAdmin)) {
      manager->sendSignal(SIGNAL_DEPARTURE);
      std::cout << "  └─ \033[33mDeparture Signal Sent.\033[0m\n";
    } else {
      printAccessDenied("Operator");
    }
  }

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
  static void printAccessDenied(const std::string &requiredRole) {
    std::cout << "  └─ \033[31mPermission Denied.\033[0m Need " << requiredRole
              << " role.\n";
  }
};
