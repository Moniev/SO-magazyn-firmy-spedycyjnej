/**
 * @file Belt.h
 * @brief Logic for the conveyor belt system with Worker management.
 *
 * This file defines the `Belt` class, which manages the central circular buffer
 * in shared memory. It acts as the synchronization point between Producers
 * (Workers) and the Consumer (Dispatcher), enforcing capacity limits (K items)
 * and weight limits (M kg).
 */
#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

/**
 * @class Belt
 * @brief Manages the shared circular buffer representing the conveyor belt.
 *
 * The Belt class implements a thread-safe (process-safe) **Circular FIFO
 * Buffer**. It handles the core Producer-Consumer logic using System V
 * Semaphores abstracted via callback functions.
 *
 * Key Responsibilities:
 * - **Worker Registration:** Tracking how many workers are currently active.
 * - **Push (Produce):** Adding packages to the belt while respecting limits
 * ($K$ and $M$).
 * - **Pop (Consume):** Removing packages from the belt for the Dispatcher.
 * - **Synchronization:** Using semaphores to block when full (Producer wait) or
 * empty (Consumer wait).
 */
class Belt {
private:
  /** @brief Pointer to the system's Shared Memory state. */
  SharedState *shm;

  /** @name Synchronization Callbacks
   * Functions injected by the Manager to handle low-level IPC operations.
   * @{ */
  std::function<void()>
      wait_empty_fn; /**< Decrements 'Empty Slots' semaphore (P operation). */
  std::function<void()>
      signal_empty_fn; /**< Increments 'Empty Slots' semaphore (V operation). */
  std::function<void()>
      wait_full_fn; /**< Decrements 'Full Slots' semaphore (P operation). */
  std::function<void()>
      signal_full_fn; /**< Increments 'Full Slots' semaphore (V operation). */
  std::function<void()>
      lock_fn; /**< Acquires the Belt Mutex (Binary Semaphore). */
  std::function<void()>
      unlock_fn; /**< Releases the Belt Mutex (Binary Semaphore). */
  /** @} */

  /**
   * @brief Simulates the time taken to place an item on the belt.
   *
   * The delay is dynamic based on the number of active workers. More workers
   * implies a slightly faster overall throughput, but this function adds a
   * small physical delay to simulate human movement.
   */
  void simulateWorkLoad() {
    if (!shm)
      return;

    if (shm->current_workers_count <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return;
    }

    int delay = 500 / shm->current_workers_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }

public:
  /**
   * @brief Constructs the Belt controller.
   *
   * @param shared_state Pointer to the Shared Memory segment.
   * @param wait_empty Callback to wait for an empty slot.
   * @param signal_empty Callback to signal a slot has been freed.
   * @param wait_full Callback to wait for a filled slot (package available).
   * @param signal_full Callback to signal a slot has been filled.
   * @param lock Callback to lock the belt mutex.
   * @param unlock Callback to unlock the belt mutex.
   */
  Belt(SharedState *shared_state, std::function<void()> wait_empty,
       std::function<void()> signal_empty, std::function<void()> wait_full,
       std::function<void()> signal_full, std::function<void()> lock,
       std::function<void()> unlock)
      : shm(shared_state), wait_empty_fn(wait_empty),
        signal_empty_fn(signal_empty), wait_full_fn(wait_full),
        signal_full_fn(signal_full), lock_fn(lock), unlock_fn(unlock) {}

  /**
   * @brief Registers a new worker on the belt.
   *
   * Checks if the belt has reached the maximum number of simultaneous workers
   * (`MAX_WORKERS_PER_BELT`). If not, increments the counter.
   *
   * @return true if registration was successful, false if the belt is full.
   */
  bool registerWorker() {
    if (!shm)
      return false;
    bool success = false;
    lock_fn();
    if (shm->current_workers_count < MAX_WORKERS_PER_BELT) {
      shm->current_workers_count++;
      spdlog::info("[belt] Worker joined. Total: {}/{}",
                   shm->current_workers_count, MAX_WORKERS_PER_BELT);
      success = true;
    }
    unlock_fn();
    return success;
  }

  /**
   * @brief Unregisters a worker from the belt.
   *
   * Decrements the active worker counter. Should be called when a worker
   * process terminates.
   */
  void unregisterWorker() {
    if (!shm)
      return;
    lock_fn();
    if (shm->current_workers_count > 0) {
      shm->current_workers_count--;
      spdlog::info("[belt] Worker left. Total: {}/{}",
                   shm->current_workers_count, MAX_WORKERS_PER_BELT);
    }
    unlock_fn();
  }

  /**
   * @brief Pushes a package onto the belt (Producer operation).
   *
   * This method:
   * 1. **Waits** for an empty slot semaphore (blocking).
   * 2. **Locks** the belt mutex.
   * 3. **Checks Limits:** Verifies `MAX_BELT_CAPACITY_K` and
   * `MAX_BELT_WEIGHT_M`.
   * - If Mass Limit ($M$) is exceeded, the package is rejected, and the worker
   * must retry.
   * 4. **Writes** the package to the circular buffer at the `tail` index.
   * 5. **Updates** statistics (Total created, Current count, Total weight).
   * 6. **Unlocks** the mutex.
   * 7. **Signals** the 'Full Slots' semaphore to wake up the Dispatcher.
   *
   * @param pkg Reference to the package to be added. Its ID is assigned inside
   * this function.
   */
  void push(Package &pkg) {
    if (!shm)
      return;

    simulateWorkLoad();

    wait_empty_fn();

    lock_fn();

    if (shm->current_items_count >= MAX_BELT_CAPACITY_K) {
      spdlog::error("[belt] REJECTED: Belt full! Count: {}/{}",
                    shm->current_items_count, MAX_BELT_CAPACITY_K);

      unlock_fn();
      signal_empty_fn();
      return;
    }

    shm->total_packages_created++;
    pkg.id = shm->total_packages_created;

    int current_tail = shm->tail;

    shm->belt[current_tail] = pkg;

    shm->tail = (current_tail + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count++;
    shm->current_belt_weight += pkg.weight;

    spdlog::info("[belt] Pushed ID {} at {}. Load: {}/{} (Workers: {})", pkg.id,
                 current_tail, shm->current_items_count, MAX_BELT_CAPACITY_K,
                 shm->current_workers_count);

    unlock_fn();
    signal_full_fn();
  }

  /**
   * @brief Pops a package from the belt (Consumer operation).
   *
   * This method:
   * 1. **Waits** for a full slot semaphore (blocking).
   * 2. **Locks** the belt mutex.
   * 3. **Reads** the package from the `head` index.
   * 4. **Clears** the memory slot (memset).
   * 5. **Updates** statistics (Decrements count and weight).
   * 6. **Unlocks** the mutex.
   * 7. **Signals** the 'Empty Slots' semaphore to wake up waiting Workers.
   *
   * @return The package retrieved from the belt.
   */
  Package pop() {
    if (!shm)
      return {};

    wait_full_fn();

    lock_fn();

    if (shm->current_items_count <= 0) {
      unlock_fn();
      signal_full_fn();
      return {};
    }

    int current_head = shm->head;

    Package pkg = shm->belt[current_head];

    std::memset(&shm->belt[current_head], 0, sizeof(Package));

    shm->head = (current_head + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count--;
    shm->current_belt_weight -= pkg.weight;

    spdlog::info("[belt] Popped ID {} from {}. Load: {}/{} (Workers: {})",
                 pkg.id, current_head, shm->current_items_count,
                 MAX_BELT_CAPACITY_K, shm->current_workers_count);

    unlock_fn();

    signal_empty_fn();
    return pkg;
  }

  /** @brief Returns the current number of items on the belt. */
  int getCount() const { return shm ? shm->current_items_count : 0; }

  /** @brief Returns the current number of active workers. */
  int getWorkerCount() const { return shm ? shm->current_workers_count : 0; }
};
