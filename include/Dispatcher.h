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
 * * The Dispatcher operates in a continuous loop:
 * 1. Pops a package from the Belt (blocking).
 * 2. Waits for a valid Truck to be present at the Dock.
 * 3. Loads the package.
 * 4. Signals departure if the truck is full.
 */
class Dispatcher {
private:
  Belt *belt;       /**< Pointer to the Belt manager. */
  SharedState *shm; /**< Pointer to shared memory. */

  std::function<void()> lock_dock_fn;   /**< Locks the dock mutex. */
  std::function<void()> unlock_dock_fn; /**< Unlocks the dock mutex. */
  std::function<void(SignalType)>
      send_signal_fn; /**< Dispatches IPC signals. */

public:
  Dispatcher(Belt *b, SharedState *s, std::function<void()> lock_dock,
             std::function<void()> unlock_dock,
             std::function<void(SignalType)> send_signal)
      : belt(b), shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

  void run() {
    spdlog::info("[dispatcher] Service started. Waiting for packages...");

    while (shm && shm->running) {
      Package pkg = belt->pop();

      if (pkg.id == 0) {
        if (!shm->running)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      bool loaded = false;
      while (!loaded && shm->running) {
        lock_dock_fn();
        TruckState &truck = shm->dock_truck;

        if (truck.is_present) {
          bool fits_qty = truck.current_load < truck.max_load;
          bool fits_wgt =
              (truck.current_weight + pkg.weight) <= truck.max_weight;

          if (fits_qty && fits_wgt) {
            truck.current_load++;
            truck.current_weight += pkg.weight;
            loaded = true;

            spdlog::info("[dispatcher] Loaded Pkg {} -> Truck #{}. Load: {}/{} "
                         "({:.1f}kg)",
                         pkg.id, truck.id, truck.current_load, truck.max_load,
                         truck.current_weight);

            if (truck.current_load >= truck.max_load ||
                truck.current_weight >= truck.max_weight) {
              spdlog::info("[dispatcher] Truck #{} full. Signalling departure.",
                           truck.id);
              send_signal_fn(SIGNAL_DEPARTURE);
            }
          } else {
            if (truck.current_load > 0) {
              spdlog::warn("[dispatcher] Pkg {} doesn't fit in Truck #{}. "
                           "Requesting new truck.",
                           pkg.id, truck.id);
              send_signal_fn(SIGNAL_DEPARTURE);
            }
          }
        }

        unlock_dock_fn();

        if (!loaded) {
          spdlog::debug("[dispatcher] Waiting for available truck...");
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
      }
    }
    spdlog::info("[dispatcher] Service stopped.");
  }
};
