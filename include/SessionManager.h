/**
 * @file SessionManager.h
 * @brief Management of active process sessions and resource quotas.
 */

#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <functional>
#include <string>
#include <unistd.h>

/**
 * @class SessionManager
 * @brief Orchestrates user logins, process quotas, and authorization within
 * IPC.
 * * The SessionManager ensures that every process connecting to the Warehouse
 * System is registered in the Shared Memory `users` table. It tracks process
 * IDs (PIDs), enforces limits on concurrent sub-processes, and manages
 * role-based access control.
 */
class SessionManager {
private:
  SharedState
      *shm; /**< Pointer to the shared state containing the session table. */
  int current_session =
      -1; /**< Index of the currently active session in SHM users array. */

  /** @name Synchronization Callbacks
   * Wrappers for semaphore operations to ensure thread-safe updates to the
   * session table.
   * @{ */
  std::function<void()> lock_fn;
  std::function<void()> unlock_fn;
  /** @} */
public:
  /**
   * @brief Constructs a SessionManager.
   * @param shared_state Pointer to SharedState.
   * @param lock Mutex lock for the session registry.
   * @param unlock Mutex unlock for the session registry.
   */
  SessionManager(SharedState *shared_state, std::function<void()> lock,
                 std::function<void()> unlock)
      : shm(shared_state), lock_fn(lock), unlock_fn(unlock) {}

  /**
   * @brief Registers a new process session in Shared Memory.
   * * Scans for duplicate usernames and available slots. If successful,
   * initializes a UserSession entry with the provided credentials and current
   * PID.
   * @param name Unique username for the session.
   * @param role Bitmask of permissions (UserRole).
   * @param orgId Identifier of the organization.
   * @param max_procs Maximum number of sub-processes this session can spawn.
   * @return true if login succeeded, false if duplicate found or limit reached.
   */
  bool login(const std::string &name, UserRole role, OrgId orgId,
             int max_procs = 5) {
    if (!shm)
      return false;

    lock_fn();

    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (shm->users[i].active &&
          std::strncmp(shm->users[i].username, name.c_str(), 31) == 0) {
        spdlog::warn("[session] User '{}' is already logged in!", name);
        unlock_fn();
        return false;
      }
    }

    int free_slot = -1;
    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (!shm->users[i].active) {
        free_slot = i;
        break;
      }
    }

    if (free_slot == -1) {
      spdlog::error(
          "[session] Session limit reached (MAX 3)! Cannot log in '{}'.", name);
      unlock_fn();
      return false;
    }

    std::memset(&shm->users[free_slot], 0, sizeof(UserSession));

    shm->users[free_slot].active = true;
    std::strncpy(shm->users[free_slot].username, name.c_str(), 31);

    shm->users[free_slot].role = role;
    shm->users[free_slot].orgId = orgId;

    shm->users[free_slot].max_processes = max_procs;
    shm->users[free_slot].current_processes = 0;
    shm->users[free_slot].session_pid = getpid();

    current_session = free_slot;

    spdlog::info("[session] Logged in: '{}' (Org: {}, RoleMask: {}) @ Slot {}",
                 name, orgId, (int)role, free_slot);

    unlock_fn();
    return true;
  }

  /**
   * @brief Marks the current session as inactive and clears security
   * credentials.
   * @note After logout, current_session is reset to -1.
   */
  void logout() {
    if (current_session == -1 || !shm)
      return;

    lock_fn();
    spdlog::info("[session] Logging out: '{}'",
                 shm->users[current_session].username);

    shm->users[current_session].active = false;

    shm->users[current_session].role = UserRole::None;
    shm->users[current_session].orgId = 0;

    shm->users[current_session].current_processes = 0;
    current_session = -1;

    unlock_fn();
  }

  /**
   * @brief Checks and increments the process quota for the current session.
   * @return true if a new process can be spawned, false if quota is exceeded.
   */
  bool trySpawnProcess() {
    if (current_session == -1 || !shm)
      return false;
    bool success = false;
    lock_fn();
    UserSession &user = shm->users[current_session];
    if (user.current_processes < user.max_processes) {
      user.current_processes++;
      success = true;
    }
    unlock_fn();
    return success;
  }

  /**
   * @brief Decrements the active process count for the current session.
   * @note Prevents underflow by checking if count > 0.
   */
  void reportProcessFinished() {
    if (current_session == -1 || !shm)
      return;
    lock_fn();
    if (shm->users[current_session].current_processes > 0) {
      shm->users[current_session].current_processes--;
    }
    unlock_fn();
  }

  /** @brief Returns the permission bitmask of the current user. */
  UserRole getCurrentRole() {
    if (current_session == -1 || !shm)
      return UserRole::None;
    return shm->users[current_session].role;
  }

  /** @brief Returns the index of the current session in SHM. */
  int getSessionIndex() const { return current_session; }
};
