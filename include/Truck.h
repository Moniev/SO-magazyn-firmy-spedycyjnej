/**
 * @file Truck.h
 * @brief Logic for the logistics vehicle lifecycle.
 */

#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

/**
 * @class Truck
 * @brief Simulates an autonomous transport vehicle driver.
 * * The Truck class operates as a state machine that cycles through:
 * 1. **Arrival**: Checks into the dock (if empty).
 * 2. **Loading**: Blocks on a message queue signal (WAITING state).
 * 3. **Departure**: Resets the dock state and increments global stats.
 * 4. **Delivery**: Simulates travel time (sleep) before returning for the next
 * shift.
 */
class Truck {
private:
  SharedState *shm; /**< Pointer to the Shared Memory segment. */

  /** @name Synchronization Hooks
   * @{ */
  std::function<void()> lock_dock_fn;   /**< Locks the Dock mutex. */
  std::function<void()> unlock_dock_fn; /**< Unlocks the Dock mutex. */
  std::function<SignalType()>
      wait_for_signal_fn; /**< Blocks execution until a departure signal is
                             received. */
  /** @} */

  /**
   * @brief Generates random capacity specifications for a new truck.
   * * Assigns a random `max_load` (5-15 items) and `max_weight` (50-150kg).
   * * Sets the `is_present` flag to true and resets current load counters.
   * @param truck Reference to the TruckState in shared memory.
   */
  void randomizeTruckSpecs(TruckState &truck) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> load_dist(5, 15);
    std::uniform_real_distribution<> weight_dist(50.0, 150.0);

    truck.id = getpid();
    truck.current_load = 0;
    truck.current_weight = 0.0;
    truck.max_load = load_dist(gen);
    truck.max_weight = weight_dist(gen);
    truck.is_present = true;
  }

public:
  /**
   * @brief Constructs a Truck driver instance.
   * @param s Pointer to SharedState.
   * @param lock_dock Callback for acquiring dock mutex.
   * @param unlock_dock Callback for releasing dock mutex.
   * @param wait_for_signal Callback for blocking message queue reception.
   */
  Truck(SharedState *s, std::function<void()> lock_dock,
        std::function<void()> unlock_dock,
        std::function<SignalType()> wait_for_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        wait_for_signal_fn(wait_for_signal) {}

  /**
   * @brief Main execution loop for the truck process.
   * * @process_flow
   * 1. **Queueing**: If the dock is occupied (`is_present == true`), waits 1s
   * and retries.
   * 2. **Docking**: Acquires lock, initializes specs via `randomizeTruckSpecs`,
   * and releases lock.
   * 3. **Waiting**: Blocks on `wait_for_signal_fn()` until `SIGNAL_DEPARTURE`
   * or `SIGNAL_END_WORK`.
   * 4. **Departing**: Updates global `trucks_completed` counter and clears
   * `is_present`.
   * 5. **Transit**: Sleeps for a random duration (2-5s) to simulate delivery.
   * * Loop continues until `shm->running` is false or `SIGNAL_END_WORK` is
   * received.
   */
  void run() {
    spdlog::info("[truck] Driver ready. Starting shift.");

    while (shm && shm->running) {
      lock_dock_fn();

      if (shm->dock_truck.is_present) {
        unlock_dock_fn();
        spdlog::debug("[truck] Dock occupied. Waiting in queue.");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }

      randomizeTruckSpecs(shm->dock_truck);
      spdlog::info("[truck] Arrived at dock. Capacity: {} items / {:.1f}kg. "
                   "Waiting for load.",
                   shm->dock_truck.max_load, shm->dock_truck.max_weight);

      unlock_dock_fn();

      SignalType sig = wait_for_signal_fn();

      if (sig == SIGNAL_END_WORK) {
        spdlog::info("[truck] End of work signal received.");
        break;
      }

      lock_dock_fn();

      spdlog::info("[truck] Departing! Transporting {} items ({:.1f}kg).",
                   shm->dock_truck.current_load,
                   shm->dock_truck.current_weight);

      shm->trucks_completed++;
      shm->dock_truck.is_present = false;

      unlock_dock_fn();

      int delivery_time = 2000 + (rand() % 3000);
      spdlog::debug("[truck] On the road. ({}ms)", delivery_time);
      std::this_thread::sleep_for(std::chrono::milliseconds(delivery_time));

      spdlog::info("[truck] Delivery complete. Returning to base.");
    }

    lock_dock_fn();
    if (shm->dock_truck.id == getpid()) {
      shm->dock_truck.is_present = false;
    }
    unlock_dock_fn();
  }
};
