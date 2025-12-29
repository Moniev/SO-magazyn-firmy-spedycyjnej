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
  SessionManager(SharedState *sharedState, std::function<void()> lock,
                 std::function<void()> unlock)
      : shm(sharedState), lock_fn(lock), unlock_fn(unlock) {}

  bool login(const std::string &name, int maxProcs) {
    if (!shm)
      return false;

    lock_fn();

    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (shm->users[i].active &&
          std::strncmp(shm->users[i].username, name.c_str(), 31) == 0) {
        spdlog::warn("User '{}' is already logged in!", name);
        unlock_fn();
        return false;
      }
    }

    int freeSlot = -1;
    for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
      if (!shm->users[i].active) {
        freeSlot = i;
        break;
      }
    }

    if (freeSlot == -1) {
      spdlog::error("Session limit reached! Cannot log in '{}'.", name);
      unlock_fn();
      return false;
    }

    std::memset(&shm->users[freeSlot], 0, sizeof(UserSession));
    shm->users[freeSlot].active = true;
    std::strncpy(shm->users[freeSlot].username, name.c_str(), 31);
    shm->users[freeSlot].max_processes = maxProcs;
    shm->users[freeSlot].current_processes = 0;
    shm->users[freeSlot].session_pid = getpid();

    current_session = freeSlot;

    spdlog::info("User '{}' logged in (Session ID: {}). Limit: {}", name,
                 freeSlot, maxProcs);

    unlock_fn();
    return true;
  }

  void logout() {
    if (current_session == -1 || !shm)
      return;

    lock_fn();

    spdlog::info("User '{}' logging out.",
                 shm->users[current_session].username);
    shm->users[current_session].active = false;
    shm->users[current_session].current_processes = 0;
    current_session = -1;

    unlock_fn();
  }

  bool trySpawnProcess() {
    if (current_session == -1 || !shm) {
      spdlog::error("No active session for this process!");
      return false;
    }

    bool success = false;
    lock_fn();

    UserSession &user = shm->users[current_session];
    if (user.current_processes < user.max_processes) {
      user.current_processes++;
      success = true;
    } else {
      spdlog::warn("Process limit reached for user '{}' ({}/{})", user.username,
                   user.current_processes, user.max_processes);
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
