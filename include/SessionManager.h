#pragma once

#include "spdlog/spdlog.h"
#include <Shared.h>
#include <cstring>
#include <functional>
#include <string>
#include <unistd.h>

class SessionManager {
private:
  SharedState *shm;

  int current_session = -1;

  std::function<void()> lock_fn;
  std::function<void()> unlock_fn;

public:
  SessionManager(SharedState *shared_state, std::function<void()> lock,
                 std::function<void()> unlock)
      : shm(shared_state), lock_fn(lock), unlock_fn(unlock) {}

  bool login(const std::string &name, int max_procs) {
    if (!shm)
      return false;

    lock_fn();

    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (shm->users[i].active &&
          std::strncmp(shm->users[i].username, name.c_str(), 31) == 0) {
        spdlog::warn("[session manager] User '{}' is already logged in!", name);
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
          "[session manager] Session limit reached! Cannot log in '{}'.", name);
      unlock_fn();
      return false;
    }

    std::memset(&shm->users[free_slot], 0, sizeof(UserSession));
    shm->users[free_slot].active = true;
    std::strncpy(shm->users[free_slot].username, name.c_str(), 31);
    shm->users[free_slot].max_processes = max_procs;
    shm->users[free_slot].current_processes = 0;
    shm->users[free_slot].session_pid = getpid();

    current_session = free_slot;

    spdlog::info(
        "[session manager] User '{}' logged in (Session ID: {}). Limit: {}",
        name, free_slot, max_procs);

    unlock_fn();
    return true;
  }

  void logout() {
    if (current_session == -1 || !shm)
      return;

    lock_fn();

    spdlog::info("[session manager] User '{}' logging out.",
                 shm->users[current_session].username);
    shm->users[current_session].active = false;
    shm->users[current_session].current_processes = 0;
    current_session = -1;

    unlock_fn();
  }

  bool trySpawnProcess() {
    if (current_session == -1 || !shm) {
      spdlog::error("[session manager] No active session for this process!");
      return false;
    }

    bool success = false;
    lock_fn();

    UserSession &user = shm->users[current_session];
    if (user.current_processes < user.max_processes) {
      user.current_processes++;
      success = true;
    } else {
      spdlog::warn(
          "[session manager] Process limit reached for user '{}' ({}/{})",
          user.username, user.current_processes, user.max_processes);
    }

    unlock_fn();
    return success;
  }

  void reportProcessFinished() {
    if (current_session == -1 || !shm)
      return;

    lock_fn();
    if (shm->users[current_session].current_processes > 0) {
      shm->users[current_session].current_processes--;
    }
    unlock_fn();
  }

  int getSessionIndex() const { return current_session; }
  void setSessionIndex(int idx) { current_session = idx; }
};
