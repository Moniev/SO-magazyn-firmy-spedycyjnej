/**
 * @file Manager.h
 * @brief Central Orchestrator for IPC resource management and component
 * coordination.
 *
 * This file defines the `Manager` class, which serves as the backbone of the
 * multi-process simulation. It handles the low-level details of System V IPC
 * (Shared Memory, Semaphores, Message Queues) and provides a high-level API
 * for components to interact with these resources.
 */

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

/**
 * @class Manager
 * @brief The heart of the Warehouse System IPC architecture.
 *
 * The Manager class acts as a **Facade** for the operating system's IPC
 * mechanisms. It is responsible for:
 * 1. **Resource Lifecycle:** Creating, attaching, and destroying Shared Memory,
 * Semaphores, and Message Queues.
 * 2. **Component Orchestration:** Initializing and holding ownership of logic
 * controllers (Belt, Truck, Dispatcher, Express, SessionManager).
 * 3. **Synchronization Primitive Abstraction:** Providing easy-to-use methods
 * for locking/unlocking mutexes (Belt, Dock) and signaling semaphores.
 * 4. **Inter-Process Communication:** Abstracting `msgsnd` and `msgrcv` for
 * signal passing.
 */
class Manager {
protected:
  /** @brief System V Shared Memory Segment ID. */
  int shm_id;

  /** @brief System V Semaphore Set ID. */
  int sem_id;

  /** @brief System V Message Queue ID. */
  int msg_id;

  /** @brief Pointer to the mapped shared memory structure. */
  SharedState *shm;

  /**
   * @brief Ownership flag.
   * If true, this process is responsible for initializing memory structures
   * on startup and marking IPC resources for destruction on exit.
   */
  bool is_owner;

public:
  /** @brief Manages user sessions and authentication logic. */
  std::unique_ptr<SessionManager> session_store;

  /** @brief Manages the conveyor belt logic (push/pop/limits). */
  std::unique_ptr<Belt> belt;

  /** @brief Manages truck behavior (docking/randomization). */
  std::unique_ptr<Truck> truck;

  /** @brief Manages the routing logic between Belt and Truck. */
  std::unique_ptr<Dispatcher> dispatcher;

  /** @brief Manages high-priority Express (P4) deliveries. */
  std::unique_ptr<Express> express;

  /**
   * @brief Constructs the Manager and initializes IPC resources.
   *
   * @param owner If true, the constructor attempts to clean up old resources,
   * creates new ones (IPC_CREAT), and initializes the SharedState structure.
   * If false, it simply connects to existing resources.
   *
   * @throws Exits the process if any IPC system call (`shmget`, `semget`,
   * `msgget`) fails.
   */
  Manager(bool owner = false) : is_owner(owner) {
    int flags = is_owner ? (IPC_CREAT | 0600) : 0600;

    if (is_owner) {
      int old_shm = shmget(SHM_KEY_ID, 0, 0);
      if (old_shm != -1) {
        shmctl(old_shm, IPC_RMID, nullptr);
      }
    }

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
      shm->current_workers_count = 0;

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
        [this](pid_t pid) { return this->receiveSignalBlocking(pid); });

    express = std::make_unique<Express>(
        shm, [this]() { this->lockDock(); }, [this]() { this->unlockDock(); },
        [this](pid_t target, SignalType s) { this->sendSignal(target, s); });

    dispatcher = std::make_unique<Dispatcher>(
        belt.get(), shm, [this]() { this->lockDock(); },
        [this]() { this->unlockDock(); },
        [this](pid_t target, SignalType s) { this->sendSignal(target, s); });
  }

  /**
   * @brief Destructor. Detaches shared memory and removes resources if owner.
   */
  virtual ~Manager() {
    if (shmdt(shm) == -1) {
      spdlog::warn("[ipc manager] shmdt failed: {}", std::strerror(errno));
    }

    if (is_owner) {
      spdlog::info("[ipc manager] Cleaning up IPC resources...");
      shmctl(shm_id, IPC_RMID, nullptr);
      semctl(sem_id, 0, IPC_RMID);
      msgctl(msg_id, IPC_RMID, nullptr);
    }
  }

  /**
   * @brief Accessor for the shared memory pointer.
   * @return Pointer to the SharedState structure.
   */
  SharedState *getState() { return shm; }

  /**
   * @brief Generic wrapper for `semop` system call.
   * Handles EINTR (interrupts) and errors gracefully.
   *
   * @param semIdx The index of the semaphore in the set (enum SemIndex).
   * @param op The operation to perform (-1 for Wait/P, +1 for Signal/V).
   */
  void semOperation(SemIndex semIdx, int op) {
    struct sembuf sb;
    sb.sem_num = static_cast<int>(semIdx);
    sb.sem_op = op;
    sb.sem_flg = 0;

    if (semop(sem_id, &sb, 1) == -1) {
      if (errno == EIDRM || errno == EINVAL) {
        if (!shm->running)
          return;
      }

      if (errno != EINTR) {
        spdlog::critical("[ipc manager] semop failed: {}",
                         std::strerror(errno));
        exit(errno);
      }
    }
  }

  /** @brief Acquires the Belt Mutex (Critical Section Entry). */
  void lockBelt() { semOperation(SEM_MUTEX_BELT, -1); }

  /** @brief Releases the Belt Mutex (Critical Section Exit). */
  void unlockBelt() { semOperation(SEM_MUTEX_BELT, 1); }

  /** @brief Decrements Empty Slots semaphore (Producer Wait). */
  void waitForEmptySlot() { semOperation(SEM_EMPTY_SLOTS, -1); }

  /** @brief Increments Empty Slots semaphore (Consumer Signal). */
  void signalSlotFreed() { semOperation(SEM_EMPTY_SLOTS, 1); }

  /** @brief Decrements Full Slots semaphore (Consumer Wait). */
  void waitForPackage() { semOperation(SEM_FULL_SLOTS, -1); }

  /** @brief Increments Full Slots semaphore (Producer Signal). */
  void signalPackageAdded() { semOperation(SEM_FULL_SLOTS, 1); }

  /** @brief Acquires the Loading Dock Mutex. */
  void lockDock() { semOperation(SEM_DOCK_MUTEX, -1); }

  /** @brief Releases the Loading Dock Mutex. */
  void unlockDock() { semOperation(SEM_DOCK_MUTEX, 1); }

  /**
   * @brief Sends a command signal to a specific process via Message Queue.
   *
   * @param target_pid The PID of the recipient process (used as mtype).
   * @param type The command to send (SignalType).
   */
  void sendSignal(pid_t target_pid, SignalType type) {
    CommandMessage msg;
    msg.mtype = target_pid;
    msg.command_id = static_cast<int>(type);

    if (msgsnd(msg_id, &msg, sizeof(int), 0) == -1) {
      spdlog::error("[ipc manager] msgsnd failed (target {}): {}", target_pid,
                    std::strerror(errno));
    } else {
      spdlog::info("[ipc manager] Signal {} sent to PID {}.", (int)type,
                   target_pid);
    }
  }

  /**
   * @brief Blocking wait for a signal addressed to this process.
   *
   * @param my_pid The PID of the calling process (used to filter messages).
   * @return The received SignalType.
   */
  SignalType receiveSignalBlocking(pid_t my_pid) {
    CommandMessage msg;
    if (msgrcv(msg_id, &msg, sizeof(int), my_pid, 0) != -1) {
      return static_cast<SignalType>(msg.command_id);
    }
    return SIGNAL_NONE;
  }

  /**
   * @brief Non-Blocking check for a signal (IPC_NOWAIT).
   *
   * @param my_pid The PID of the calling process.
   * @return The received SignalType, or SIGNAL_NONE if queue is empty.
   */
  SignalType receiveSignalNonBlocking(pid_t my_pid) {
    CommandMessage msg;
    if (msgrcv(msg_id, &msg, sizeof(int), my_pid, IPC_NOWAIT) != -1) {
      return static_cast<SignalType>(msg.command_id);
    }
    return SIGNAL_NONE;
  }
};
