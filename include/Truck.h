/**
 * @file Truck.h
 * @brief Defines the autonomous Truck agent responsible for cargo transport.
 *
 * This file contains the implementation of the Truck process. Trucks act as
 * transient consumers of the loading dock resource. They arrive, wait to be
 * loaded by the Dispatcher (or P4), and depart upon receiving a signal or
 * becoming full.
 */

#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>
#include <thread>
#include <unistd.h>

/**
 * @class Truck
 * @brief Represents a delivery vehicle in the logistic system.
 *
 * The Truck class implements the lifecycle of a vehicle:
 * 1. **Arrival:** Waiting for the dock to become free.
 * 2. **Docking:** Occupying the dock and establishing capacity limits (W, V).
 * 3. **Loading:** Blocking wait state while Dispatcher/P4 load packages.
 * 4. **Departure:** Triggered by capacity limits or explicit signals.
 * 5. **Delivery:** Simulation of travel time ($T_i$).
 */
class Truck {
private:
  /** @brief Pointer to the shared memory segment containing system state. */
  SharedState *shm;

  /** @brief Process ID of this specific truck instance. */
  pid_t my_pid;

  /** @name IPC Callbacks
   * Abstracted functions to interact with System V IPC semaphores and queues.
   * @{ */
  std::function<void()> lock_dock_fn;   /**< Acquires the dock mutex. */
  std::function<void()> unlock_dock_fn; /**< Releases the dock mutex. */
  std::function<SignalType(pid_t)>
      wait_for_signal_fn; /**< Blocks waiting for a message. */
  /** @} */

  /**
   * @brief Generates random specifications for the truck upon arrival.
   *
   * Sets the truck's unique constraints for the current trip:
   * - **Max Weight (W):** Randomly selected between 200.0 kg and 600.0 kg.
   * - **Max Volume (V):** Randomly selected between 1.0 m³ and 3.0 m³.
   * - **ID:** Sets the current dock occupant ID to this process's PID.
   *
   * @param truck Reference to the shared memory truck state structure.
   */
  void randomizeTruckSpecs(TruckState &truck) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_real_distribution<> weight_cap_dist(200.0, 600.0);
    std::uniform_real_distribution<> vol_cap_dist(1.0, 3.0);

    truck.id = my_pid;

    truck.current_load = 0;
    truck.current_weight = 0.0;
    truck.current_volume = 0.0;

    truck.max_load = 100;
    truck.max_weight = weight_cap_dist(gen);
    truck.max_volume = vol_cap_dist(gen);

    truck.is_present = true;
  }

public:
  /**
   * @brief Constructs a new Truck instance.
   *
   * @param s Pointer to shared memory.
   * @param lock_dock Callback to lock the dock semaphore.
   * @param unlock_dock Callback to unlock the dock semaphore.
   * @param wait_for_signal Callback to receive IPC messages blocking.
   */
  Truck(SharedState *s, std::function<void()> lock_dock,
        std::function<void()> unlock_dock,
        std::function<SignalType(pid_t)> wait_for_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        wait_for_signal_fn(wait_for_signal) {
    my_pid = getpid();
  }

  /**
   * @brief Main operational loop of the Truck.
   *
   * This method executes the continuous cycle of the transport vehicle:
   * 1. **Queueing:** Checks if the dock is free. If occupied, sleeps for 1s.
   * 2. **Docking:** If free, acquires the dock and calls `randomizeTruckSpecs`.
   * 3. **Service Wait:** Blocks on `wait_for_signal_fn`. The truck does
   * *nothing* while waiting here; it is purely passive, being loaded by other
   * processes.
   * 4. **Signal Handling:**
   * - `SIGNAL_DEPARTURE`: Truck leaves the dock to deliver goods.
   * - `SIGNAL_END_WORK`: Truck terminates operations.
   * 5. **Departure:** Updates statistics (`trucks_completed`) and clears dock
   * state.
   * 6. **Delivery Simulation:** Sleeps for a random duration ($T_i$) between
   * 3-8 seconds.
   *
   * @note This function runs until `SIGNAL_END_WORK` is received or
   * `shm->running` becomes false.
   */
  void run() {
    spdlog::info("[truck-{}] Engine started. Joining fleet.", my_pid);

    while (shm && shm->running) {
      lock_dock_fn();

      if (shm->dock_truck.is_present) {
        unlock_dock_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }

      randomizeTruckSpecs(shm->dock_truck);
      spdlog::info(
          "[truck-{}] Docked. Max W:{:.1f}kg, Max V:{:.3f}m3. Waiting.", my_pid,
          shm->dock_truck.max_weight, shm->dock_truck.max_volume);

      unlock_dock_fn();

      SignalType sig = wait_for_signal_fn(my_pid);

      if (sig == SIGNAL_END_WORK || !shm->running) {
        break;
      }

      lock_dock_fn();

      if (shm->dock_truck.id == my_pid) {
        shm->trucks_completed++;
        shm->dock_truck.is_present = false;

        spdlog::info("[truck-{}] Departing. Payload: {:.1f}kg / {:.3f}m3. "
                     "Total dispatched: {}",
                     my_pid, shm->dock_truck.current_weight,
                     shm->dock_truck.current_volume, shm->trucks_completed);
      } else {
        spdlog::critical("[truck-{}] ERROR: Identity theft at dock!", my_pid);
      }

      unlock_dock_fn();

      int route_time = 3000 + (rand() % 5000);
      spdlog::info("[truck-{}] On route... returning in {}ms", my_pid,
                   route_time);
      std::this_thread::sleep_for(std::chrono::milliseconds(route_time));
    }

    lock_dock_fn();
    if (shm->dock_truck.is_present && shm->dock_truck.id == my_pid) {
      shm->dock_truck.is_present = false;
    }
    unlock_dock_fn();

    spdlog::info("[truck-{}] Shift ended.", my_pid);
  }
};
