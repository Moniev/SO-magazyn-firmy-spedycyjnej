#pragma once

#include "Belt.h"
#include "Dispatcher.h"
#include "Express.h"
#include "SessionManager.h"
#include "Shared.h"
#include "Truck.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

class Manager {
protected:
  int shm_id;
  int sem_id;
  int msg_id;
  SharedState *shm;
  bool is_owner;

public:
  std::unique_ptr<SessionManager> session_store;
  std::unique_ptr<Belt> belt;
  std::unique_ptr<Truck> truck;
  std::unique_ptr<Dispatcher> dispatcher;
  std::unique_ptr<Express> express;

  Manager(bool owner = false) : is_owner(owner) {
    int flags = is_owner ? (IPC_CREAT | 0600) : 0600;

    shm_id = shmget(SHM_KEY_ID, sizeof(SharedState), flags);
    if (shm_id == -1) {
      spdlog::critical("[ipc manager] shmget failed: {}", std::strerror(errno));
      exit(errno);
    }

    shm = (SharedState *)shmat(shm_id, nullptr, 0);
    if (shm == (void *)-1) {
      spdlog::critical("[ipc manager] shmat failed: {}", std::strerror(errno));
      exit(errno);
    }

    sem_id = semget(SEM_KEY_ID, is_owner ? SEM_TOTAL : 0, flags);
    if (sem_id == -1) {
      spdlog::critical("[ipc manager] semget failed: {}", std::strerror(errno));
      exit(errno);
    }

    msg_id = msgget(MSG_KEY_ID, flags);
    if (msg_id == -1) {
      spdlog::critical("[ipc manager] msgget failed: {}", std::strerror(errno));
      exit(errno);
    }

    if (is_owner) {
      std::memset(shm, 0, sizeof(SharedState));

      shm->running = true;
      shm->total_packages_created = 0;
      shm->trucks_completed = 0;

      semctl(sem_id, SEM_MUTEX_BELT, SETVAL, 1);
      semctl(sem_id, SEM_DOCK_MUTEX, SETVAL, 1);
      semctl(sem_id, SEM_EMPTY_SLOTS, SETVAL, MAX_BELT_CAPACITY_K);
      semctl(sem_id, SEM_FULL_SLOTS, SETVAL, 0);

      spdlog::info(
          "[ipc manager] IPC Initialized: SHM ID {}, SEM ID {}, MSG ID {}",
          shm_id, sem_id, msg_id);
    }

    session_store = std::make_unique<SessionManager>(
        shm, [this]() { this->lockBelt(); }, [this]() { this->unlockBelt(); });

    belt = std::make_unique<Belt>(
        shm, [this]() { this->waitForEmptySlot(); },
        [this]() { this->signalSlotFreed(); },
        [this]() { this->waitForPackage(); },
        [this]() { this->signalPackageAdded(); },
        [this]() { this->lockBelt(); }, [this]() { this->unlockBelt(); });

    truck = std::make_unique<Truck>(
        shm, [this]() { this->lockDock(); }, [this]() { this->unlockDock(); },
        [this]() { return this->receiveSignalBlocking(); });

    express = std::make_unique<Express>(
        shm, [this]() { this->lockDock(); }, [this]() { this->unlockDock(); },
        [this]() { this->lockBelt(); }, [this]() { this->unlockBelt(); },
        [this](SignalType s) { this->sendSignal(s); });

    dispatcher = std::make_unique<Dispatcher>(
        belt.get(), shm, [this]() { this->lockDock(); },
        [this]() { this->unlockDock(); },
        [this](SignalType s) { this->sendSignal(s); });
  }

  virtual ~Manager() {
    if (shmdt(shm) == -1) {
      spdlog::warn("[ipc manager] shmdt failed: {}", std::strerror(errno));
    }

    if (is_owner) {
      spdlog::info("[ipc manager] Cleaning up IPC resources...");
      if (shmctl(shm_id, IPC_RMID, nullptr) == -1)
        spdlog::warn("[ipc manager] shmctl RMID failed: {}",
                     std::strerror(errno));
      if (semctl(sem_id, 0, IPC_RMID) == -1)
        spdlog::warn("[ipc manager] semctl RMID failed: {}",
                     std::strerror(errno));
      if (msgctl(msg_id, IPC_RMID, nullptr) == -1)
        spdlog::warn("[ipc manager] msgctl RMID failed: {}",
                     std::strerror(errno));
    }
  }

  SharedState *getState() { return shm; }

  void semOperation(SemIndex semIdx, int op) {
    struct sembuf sb;
    sb.sem_num = static_cast<int>(semIdx);
    sb.sem_op = op;
    sb.sem_flg = 0;

    while (semop(sem_id, &sb, 1) == -1) {
      if (errno != EINTR) {
        spdlog::critical("[ipc manager] semop failed (idx: {}, op: {}): {}",
                         (int)semIdx, op, std::strerror(errno));
        exit(errno);
      }
    }
  }

  void lockBelt() { semOperation(SEM_MUTEX_BELT, -1); }
  void unlockBelt() { semOperation(SEM_MUTEX_BELT, 1); }

  void waitForEmptySlot() { semOperation(SEM_EMPTY_SLOTS, -1); }
  void signalSlotFreed() { semOperation(SEM_EMPTY_SLOTS, 1); }

  void waitForPackage() { semOperation(SEM_FULL_SLOTS, -1); }
  void signalPackageAdded() { semOperation(SEM_FULL_SLOTS, 1); }

  void lockDock() { semOperation(SEM_DOCK_MUTEX, -1); }
  void unlockDock() { semOperation(SEM_DOCK_MUTEX, 1); }

  void sendSignal(SignalType type) {
    CommandMessage msg;
    msg.mtype = 1;
    msg.command_id = static_cast<int>(type);

    if (msgsnd(msg_id, &msg, sizeof(int), 0) == -1) {
      spdlog::error("[ipc manager] msgsnd failed: {}", std::strerror(errno));
    } else {
      spdlog::info("[ipc manager] Signal {} sent.", (int)type);
    }
  }

  SignalType receiveSignalBlocking() {
    CommandMessage msg;
    if (msgrcv(msg_id, &msg, sizeof(int), 1, 0) != -1) {
      return static_cast<SignalType>(msg.command_id);
    }
    return SIGNAL_NONE;
  }

  SignalType receiveSignalNonBlocking() {
    CommandMessage msg;
    if (msgrcv(msg_id, &msg, sizeof(int), 1, IPC_NOWAIT) != -1) {
      return static_cast<SignalType>(msg.command_id);
    }
    return SIGNAL_NONE;
  }
};
