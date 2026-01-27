/**
 * @file Dispatcher.h
 * @brief Defines the Dispatcher agent responsible for belt-to-truck transfers.
 *
 * This file contains the logic for the Dispatcher process. The Dispatcher acts
 * as the **Consumer** of the Conveyor Belt and the **Loader** for the Docking
 * Bay. It enforces physical constraints (Weight, Volume) and orchestrates truck
 * departures.
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
 * @brief Manages the transfer of packages from the internal belt to external
 * transport.
 *
 * The Dispatcher operates as a bridge between the warehouse internals and the
 * logistics fleet. Its primary responsibilities are:
 * 1. **De-queueing:** Removing packages from the conveyor belt (FIFO).
 * 2. **Constraint Validation:** ensuring a package fits within the Truck's
 * remaining Capacity ($W$) and Volume ($V$).
 * 3. **Loading:** Updating the shared memory state of the truck.
 * 4. **Fleet Control:** Signaling trucks to depart when full or when a package
 * cannot fit.
 */
class Dispatcher {
private:
  /** @brief Pointer to the Belt subsystem (source of packages). */
  Belt *belt;

  /** @brief Pointer to the system's Shared Memory state. */
  SharedState *shm;

  /** @name IPC Synchronization Callbacks
   * Functions injected to handle synchronization without tight coupling to the
   * Manager.
   * @{ */
  std::function<void()> lock_dock_fn;   /**< Acquires Dock Mutex. */
  std::function<void()> unlock_dock_fn; /**< Releases Dock Mutex. */
  std::function<void(pid_t, SignalType)>
      send_signal_fn; /**< Sends IPC signals. */
                      /** @} */

public:
  /**
   * @brief Constructs a new Dispatcher instance.
   *
   * @param b Pointer to the Belt object.
   * @param s Pointer to Shared Memory.
   * @param lock_dock Callback for locking the dock.
   * @param unlock_dock Callback for unlocking the dock.
   * @param send_signal Callback for sending signals to trucks.
   */
  Dispatcher(Belt *b, SharedState *s, std::function<void()> lock_dock,
             std::function<void()> unlock_dock,
             std::function<void(pid_t, SignalType)> send_signal)
      : belt(b), shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

  /**
   * @brief Processes a single package from the belt.
   *
   * This method implements the core routing algorithm:
   * 1. **Pop:** Retrieves a package from the belt (blocking if empty).
   * 2. **Load Loop:** Attempts to load the package onto the current truck.
   * - **Constraint Check:** Verifies if `current + new <= max` for both Weight
   * and Volume.
   * - **Success:** Updates truck state. If truck reaches ~99% capacity or max
   * item count, sends `SIGNAL_DEPARTURE`.
   * - **Failure (Does not fit):** Sends `SIGNAL_DEPARTURE` to force the full
   * truck away, then waits for a new truck.
   * 3. **Retry:** Loops until the package is successfully loaded onto a
   * (potentially new) truck.
   *
   * @note This method handles the synchronization critical section for the
   * Dock.
   */
  void processNextPackage() {
    Package pkg = belt->pop();

    if (pkg.id == 0) {
      if (shm->running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      return;
    }

    bool loaded = false;

    while (!loaded && shm->running) {
      lock_dock_fn();
      TruckState &truck = shm->dock_truck;

      if (truck.is_present) {
        bool fits_weight =
            (truck.current_weight + pkg.weight <= truck.max_weight);
        bool fits_volume =
            (truck.current_volume + pkg.volume <= truck.max_volume);

        if (fits_weight && fits_volume) {
          truck.current_weight += pkg.weight;
          truck.current_volume += pkg.volume;
          truck.current_load++;
          loaded = true;

          spdlog::info("[dispatcher] Loaded Pkg {} ({:.1f}kg, {:.3f}m3) -> "
                       "Truck #{}. State: {:.1f}/{} kg, {:.3f}/{} m3",
                       pkg.id, pkg.weight, pkg.volume, truck.id,
                       truck.current_weight, truck.max_weight,
                       truck.current_volume, truck.max_volume);

          if (truck.current_load >= truck.max_load ||
              truck.current_weight >= truck.max_weight * 0.99 ||
              truck.current_volume >= truck.max_volume * 0.99) {

            spdlog::info("[dispatcher] Truck #{} FULL (Limit reached). Sending "
                         "DEPARTURE.",
                         truck.id);
            send_signal_fn(truck.id, SIGNAL_DEPARTURE);
          }
        } else {
          std::string reason = !fits_weight ? "Weight Limit" : "Volume Limit";
          if (!fits_weight && !fits_volume)
            reason = "Weight & Volume Limit";

          spdlog::warn("[dispatcher] Pkg {} doesn't fit in Truck #{} ({}). "
                       "Forcing departure.",
                       pkg.id, truck.id, reason);

          send_signal_fn(truck.id, SIGNAL_DEPARTURE);
        }
      }

      unlock_dock_fn();

      if (!loaded) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }
  }

  /**
   * @brief Main service loop.
   *
   * Continuously processes packages until the shared memory state indicates
   * the system is no longer running.
   */
  void run() {
    spdlog::info("[dispatcher] Service started. Controlling the dock.");

    while (shm && shm->running) {
      processNextPackage();
    }

    spdlog::info("[dispatcher] Service stopped.");
  }
};
