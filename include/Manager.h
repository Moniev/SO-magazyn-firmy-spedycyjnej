#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

class Manager {
protected:
  int shmId;
  int semId;
  int msgId;
  SharedState *shm;
  bool isOwner;

public:
  Manager(bool owner = false) : isOwner(owner) { initResources(); }

  virtual ~Manager() {
    if (shmdt(shm) == -1) {
      spdlog::warn("shmdt failed: {}", std::strerror(errno));
    }

    if (isOwner) {
      spdlog::info("Cleaning up IPC resources...");
      if (shmctl(shmId, IPC_RMID, nullptr) == -1)
        spdlog::warn("shmctl RMID failed: {}", std::strerror(errno));
      if (semctl(semId, 0, IPC_RMID) == -1)
        spdlog::warn("semctl RMID failed: {}", std::strerror(errno));
      if (msgctl(msgId, IPC_RMID, nullptr) == -1)
        spdlog::warn("msgctl RMID failed: {}", std::strerror(errno));
    }
  }

  SharedState *getState() { return shm; }

  void semOperation(SemIndex semIdx, int op) {
    struct sembuf sb;
    sb.sem_num = static_cast<int>(semIdx);
    sb.sem_op = op;
    sb.sem_flg = 0;

    while (semop(semId, &sb, 1) == -1) {
      if (errno != EINTR) {
        spdlog::critical("semop failed (idx: {}, op: {}): {}", (int)semIdx, op,
                         std::strerror(errno));
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

    if (msgsnd(msgId, &msg, sizeof(int), 0) == -1) {
      spdlog::error("msgsnd failed: {}", std::strerror(errno));
    } else {
      spdlog::info("Signal {} sent.", (int)type);
    }
  }

  SignalType receiveSignalNonBlocking() {
    CommandMessage msg;
    if (msgrcv(msgId, &msg, sizeof(int), 1, IPC_NOWAIT) != -1) {
      return static_cast<SignalType>(msg.command_id);
    }
    return SIGNAL_NONE;
  }

private:
  void initResources() {
    int flags = isOwner ? (IPC_CREAT | 0666) : 0666;

    shmId = shmget(SHM_KEY_ID, sizeof(SharedState), flags);
    if (shmId == -1) {
      spdlog::critical("shmget failed: {}", std::strerror(errno));
      exit(errno);
    }

    shm = (SharedState *)shmat(shmId, nullptr, 0);
    if (shm == (void *)-1) {
      spdlog::critical("shmat failed: {}", std::strerror(errno));
      exit(errno);
    }

    semId = semget(SEM_KEY_ID, isOwner ? SEM_TOTAL : 0, flags);
    if (semId == -1) {
      spdlog::critical("semget failed: {}", std::strerror(errno));
      exit(errno);
    }

    msgId = msgget(MSG_KEY_ID, flags);
    if (msgId == -1) {
      spdlog::critical("msgget failed: {}", std::strerror(errno));
      exit(errno);
    }

    if (isOwner) {
      std::memset(shm, 0, sizeof(SharedState));

      shm->running = true;
      shm->total_packages_created = 0;
      shm->trucks_completed = 0;

      semctl(semId, SEM_MUTEX_BELT, SETVAL, 1);
      semctl(semId, SEM_DOCK_MUTEX, SETVAL, 1);

      semctl(semId, SEM_EMPTY_SLOTS, SETVAL, MAX_BELT_CAPACITY_K);

      semctl(semId, SEM_FULL_SLOTS, SETVAL, 0);

      spdlog::info("IPC Initialized: SHM ID {}, SEM ID {}, MSG ID {}", shmId,
                   semId, msgId);
    }
  }
};
