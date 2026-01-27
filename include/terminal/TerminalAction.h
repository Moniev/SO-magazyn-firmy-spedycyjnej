/**
 * @file TerminalAction.h
 * @brief Implementation of CLI command logic, security authorization, and IPC
 * signal dispatching.
 *
 * This file defines the `TerminalActions` class, which serves as the
 * "Controller" for the terminal interface. It translates high-level user
 * commands (e.g., "vip", "stop") into low-level System V IPC signals, ensuring
 * the user has the correct privileges before execution.
 */
#pragma once

#include "../Manager.h"
#include "../Shared.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <string>

/**
 * @class TerminalActions
 * @brief Static helper class enclosing the business logic for terminal
 * commands.
 *
 * This class isolates the command execution logic from the UI loop.
 * It is responsible for:
 * 1. **Authorization:** Verifying `UserRole` against the requested action.
 * 2. **Target Resolution:** Finding the correct PID (e.g., finding the Truck or
 * the Express Worker).
 * 3. **Signal Dispatch:** Sending the appropriate IPC message to the target.
 */
class TerminalActions {
public:
  /**
   * @brief Handles the 'vip' command (Requirement: Signal 2).
   *
   * Triggers the Express Worker (P4) to load a batch of priority packages.
   *
   * **Logic:**
   * 1. Checks if the user is an **Operator** or **SysAdmin**.
   * 2. Scans the session table for the process named "System-Express".
   * 3. Sends `SIGNAL_EXPRESS_LOAD` to that process.
   *
   * @param manager Pointer to the central Manager for IPC access.
   * @param role The role of the currently logged-in user.
   */
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

  /**
   * @brief Handles the 'depart' command (Requirement: Signal 1).
   *
   * Forces the truck currently docked to leave immediately, regardless of load
   * level.
   *
   * **Logic:**
   * 1. Checks if the user is an **Operator** or **SysAdmin**.
   * 2. Locks shared memory to safely read the dock state.
   * 3. Checks if a truck is present (`dock_truck.is_present`).
   * 4. Sends `SIGNAL_DEPARTURE` to the truck's PID.
   *
   * @param manager Pointer to the central Manager for IPC access.
   * @param role The role of the currently logged-in user.
   */
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

  /**
   * @brief Handles the 'stop' command (Requirement: Signal 3).
   *
   * Initiates a global emergency shutdown of the simulation.
   *
   * **Logic:**
   * 1. Checks if the user is a **SysAdmin** (Strict check).
   * 2. Sets the global shared memory flag `running` to false.
   * 3. Iterates through the Session Table and sends `SIGNAL_END_WORK` to all
   * active processes.
   * 4. Updates the local `activeRef` to close the terminal UI.
   *
   * @param manager Pointer to the central Manager.
   * @param role The role of the currently logged-in user.
   * @param activeRef Reference to the terminal's main loop flag (set to false
   * on success).
   */
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
  /**
   * @brief Utility to print a standardized red "Permission Denied" message.
   * @param requiredRole The name of the role required to perform the action.
   */
  static void printAccessDenied(const std::string &requiredRole) {
    std::cout << "  └─ \033[31mPermission Denied.\033[0m Need " << requiredRole
              << ".\n";
  }

  /**
   * @brief Helper to find a process ID by its username in the session table.
   *
   * Used primarily to locate the "System-Express" worker process.
   *
   * @param manager Pointer to the Manager.
   * @param name The username to search for.
   * @return pid_t The process ID if found, otherwise -1.
   */
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
