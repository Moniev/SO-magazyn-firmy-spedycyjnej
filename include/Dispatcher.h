/**
 * @file Dispatcher.h
 * @brief Logic for routing packages from the belt to the loading dock.
 */

#pragma once

#include "Belt.h"
#include "Shared.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <functional>
#include <thread>

/**
 * @class Dispatcher
 * @brief Acts as the bridge between the Conveyor Belt and the Trucking system.
 * * The Dispatcher is a consumer process that pops packages from the Belt's
 * circular buffer and attempts to load them into the truck currently stationed
 * at the dock. It handles synchronization between these two distinct
 * sub-systems (Belt IPC and Dock IPC).
 */
class Dispatcher {
private:
  Belt *belt;       /**< Pointer to the Belt manager for popping data. */
  SharedState *shm; /**< Pointer to the shared state for dock updates. */

  std::function<void()> lock_dock_fn;   /**< Locks the dock mutex. */
  std::function<void()> unlock_dock_fn; /**< Unlocks the dock mutex. */
  std::function<void(SignalType)>
      send_signal_fn; /**< Dispatches IPC signals (e.g., DEPARTURE). */

public:
  /**
   * @brief Constructs a Dispatcher with necessary IPC hooks.
   * @param b Pointer to an initialized Belt instance.
   * @param s Pointer to the SharedState structure.
   * @param lock_dock Mutex lock for the dock critical section.
   * @param unlock_dock Mutex unlock for the dock critical section.
   * @param send_signal Signal dispatching hook (via Message Queue).
   */
  Dispatcher(Belt *b, SharedState *s, std::function<void()> lock_dock,
             std::function<void()> unlock_dock,
             std::function<void(SignalType)> send_signal)
      : belt(b), shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

  /**
   * @brief Orchestrates the transfer of a single package.
   * * This method implements a retry loop to ensure that once a package is
   * popped from the belt, it is eventually loaded.
   * * @process_flow
   * 1. Blocks on Belt::pop() until a package is available.
   * 2. Acquires the Dock Mutex.
   * 3. Checks if a truck is present and has capacity.
   * 4. Updates truck metrics (count and weight).
   * 5. If capacity is reached, triggers a SIGNAL_DEPARTURE.
   * 6. Releases Dock Mutex.
   * * @note If no truck is present or the truck is full, the method waits
   * and retries, ensuring no package data is lost after being removed from the
   * belt.
   */
  void processNextPackage() {
    if (!belt || !shm)
      return;

    Package pkg = belt->pop();

    if (pkg.id == 0)
      return;

    bool package_loaded = false;

    while (!package_loaded && shm->running) {
      lock_dock_fn();

      TruckState &truck = shm->dock_truck;

      if (truck.is_present) {
        bool fits_qty = truck.current_load < truck.max_load;
        bool fits_wgt = (truck.current_weight + pkg.weight) <= truck.max_weight;

        if (fits_qty && fits_wgt) {
          truck.current_load++;
          truck.current_weight += pkg.weight;

          spdlog::info(
              "[dispatcher] [belt] Popped ID {} -> Truck #{}. Load: {}/{}",
              pkg.id, truck.id, truck.current_load, truck.max_load);

          package_loaded = true;

          if (truck.current_load >= truck.max_load) {
            send_signal_fn(SIGNAL_DEPARTURE);
          }
        } else {
          if (truck.current_load >= truck.max_load) {
            spdlog::debug("[dispatcher] Truck full, signalling departure.");
            send_signal_fn(SIGNAL_DEPARTURE);
          }
        }
      }

      unlock_dock_fn();

      if (!package_loaded) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }
  }
};
