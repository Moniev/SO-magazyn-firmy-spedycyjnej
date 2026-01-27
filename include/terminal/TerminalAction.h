/**
 * @file TerminalAction.h
 * @brief Implementation of CLI command logic and security checks.
 */
#pragma once

#include "../Manager.h"
#include "../Shared.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <string>

class TerminalActions {
public:
  static void handleVip(Manager *manager, UserRole role) {
    if (!hasFlag(role, UserRole::Operator) &&
        !hasFlag(role, UserRole::SysAdmin)) {
      printAccessDenied("Operator");
      return;
    }

    pid_t target = findProcessByRole(manager, "System-Express");

    if (target > 0) {
      manager->sendSignal(target, SIGNAL_EXPRESS_LOAD);
      std::cout << "  └─ \033[36mVIP Request Sent to PID " << target
                << ".\033[0m\n";
    } else {
      spdlog::error("[cli] VIP Service (Express) not found in session table!");
      std::cout << "  └─ \033[31mError: Express service offline.\033[0m\n";
    }
  }

  static void handleDepart(Manager *manager, UserRole role) {
    if (!hasFlag(role, UserRole::Operator) &&
        !hasFlag(role, UserRole::SysAdmin)) {
      printAccessDenied("Operator");
      return;
    }

    SharedState *shm = manager->getState();
    if (shm->dock_truck.is_present) {
      pid_t truck_pid = shm->dock_truck.id;
      manager->sendSignal(truck_pid, SIGNAL_DEPARTURE);
      std::cout << "  └─ \033[33mDeparture Signal Sent to Truck PID "
                << truck_pid << ".\033[0m\n";
    } else {
      std::cout << "  └─ \033[31mNo truck in dock to depart.\033[0m\n";
    }
  }

  static void handleStop(Manager *manager, UserRole role, bool &activeRef) {
    if (hasFlag(role, UserRole::SysAdmin)) {
      spdlog::critical("[cli] EMERGENCY STOP INITIATED BY ADMIN");

      SharedState *shm = manager->getState();
      manager->getState()->running = false;

      for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
        if (shm->users[i].active) {
          manager->sendSignal(shm->users[i].session_pid, SIGNAL_END_WORK);
        }
      }

      std::cout << "  └─ \033[31mSYSTEM-WIDE HALT COMMANDED.\033[0m\n";
      activeRef = false;
    } else {
      spdlog::warn("[security] Unauthorized stop attempt from Role: {}",
                   (int)role);
      printAccessDenied("SysAdmin");
    }
  }

private:
  static void printAccessDenied(const std::string &requiredRole) {
    std::cout << "  └─ \033[31mPermission Denied.\033[0m Need " << requiredRole
              << ".\n";
  }

  static pid_t findProcessByRole(Manager *manager, const std::string &name) {
    SharedState *shm = manager->getState();
    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (shm->users[i].active && std::string(shm->users[i].username) == name) {
        return shm->users[i].session_pid;
      }
    }
    return -1;
  }
};
