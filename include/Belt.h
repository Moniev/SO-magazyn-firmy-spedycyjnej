/**
 * @file Belt.h
 * @brief Logic for the conveyor belt system using a Shared Memory circular
 * buffer.
 */

#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <functional>

/**
 * @class Belt
 * @brief Manages package flow on the conveyor belt.
 * * This class implements a thread-safe (via semaphores) circular buffer
 * located in Shared Memory. It follows the Producer-Consumer pattern where the
 * Belt process acts as a producer and the Dispatcher acts as a consumer.
 */
class Belt {
private:
  /** @brief Pointer to the mapped Shared Memory segment. */
  SharedState *shm;

  /** @name Synchronization Callbacks
   * Functional wrappers for semaphore operations provided by the Manager.
   * @{ */
  std::function<void()>
      wait_empty_fn; /**< Blocks until an empty slot is available. */
  std::function<void()>
      signal_empty_fn; /**< Signals that a slot has been freed. */
  std::function<void()>
      wait_full_fn; /**< Blocks until a package is available to pop. */
  std::function<void()>
      signal_full_fn; /**< Signals that a new package has been added. */
  std::function<void()>
      lock_fn; /**< Acquires the mutex for critical section. */
  std::function<void()>
      unlock_fn; /**< Releases the mutex for critical section. */
                 /** @} */

public:
  /**
   * @brief Constructs a Belt instance with required synchronization hooks.
   * @param shared_state Pointer to the SharedState structure.
   * @param wait_empty Callback for SEM_EMPTY_SLOTS.
   * @param signal_empty Callback for SEM_EMPTY_SLOTS.
   * @param wait_full Callback for SEM_FULL_SLOTS.
   * @param signal_full Callback for SEM_FULL_SLOTS.
   * @param lock Callback for SEM_MUTEX_BELT.
   * @param unlock Callback for SEM_MUTEX_BELT.
   */
  Belt(SharedState *shared_state, std::function<void()> wait_empty,
       std::function<void()> signal_empty, std::function<void()> wait_full,
       std::function<void()> signal_full, std::function<void()> lock,
       std::function<void()> unlock)
      : shm(shared_state), wait_empty_fn(wait_empty),
        signal_empty_fn(signal_empty), wait_full_fn(wait_full),
        signal_full_fn(signal_full), lock_fn(lock), unlock_fn(unlock) {}

  /**
   * @brief Pushes a package onto the circular buffer.
   * * The method blocks if the belt is at maximum capacity
   * (MAX_BELT_CAPACITY_K). It updates the tail pointer and increments global
   * system counters.
   * * @param pkg Reference to the Package to be added. The ID is assigned
   * internally.
   * @note Thread-safe: Uses SEM_EMPTY_SLOTS and SEM_MUTEX_BELT.
   */
  void push(Package &pkg) {
    if (!shm)
      return;

    wait_empty_fn();
    lock_fn();

    shm->total_packages_created++;
    pkg.id = shm->total_packages_created;

    int current_tail = shm->tail;
    shm->belt[current_tail] = pkg;

    shm->tail = (current_tail + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count++;
    shm->current_belt_weight += pkg.weight;

    spdlog::info("[belt] Pushed ID {} at {}. Load: {}/{}", pkg.id, current_tail,
                 shm->current_items_count, MAX_BELT_CAPACITY_K);

    unlock_fn();
    signal_full_fn();
  }

  /**
   * @brief Pops a package from the circular buffer.
   * * The method blocks if the belt is empty. It retrieves the package at
   * the current head index and clears the memory slot.
   * * @return Package The retrieved package data. Returns an empty Package if
   * SHM is null.
   * @note Thread-safe: Uses SEM_FULL_SLOTS and SEM_MUTEX_BELT.
   */
  Package pop() {
    if (!shm)
      return {};

    wait_full_fn();
    lock_fn();

    int current_head = shm->head;
    Package pkg = shm->belt[current_head];

    std::memset(&shm->belt[current_head], 0, sizeof(Package));

    shm->head = (current_head + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count--;
    shm->current_belt_weight -= pkg.weight;

    spdlog::info("[belt] Popped ID {} from {}. Load: {}/{}", pkg.id,
                 current_head, shm->current_items_count, MAX_BELT_CAPACITY_K);

    unlock_fn();
    signal_empty_fn();

    return pkg;
  }

  /**
   * @brief Returns the current number of items on the belt.
   * @return int Count of packages currently in the buffer.
   */
  int getCount() const { return shm ? shm->current_items_count : 0; }
};
