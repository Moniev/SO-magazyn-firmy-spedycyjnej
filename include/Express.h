/**
 * @file Express.h
 * @brief Logic for the P4 Worker (Express/VIP Delivery).
 *
 * This file defines the `Express` class, which handles high-priority package
 * delivery. Unlike standard workers, the Express worker bypasses the conveyor
 * belt and loads packages directly into the truck upon receiving a specific
 * signal.
 */
#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

/**
 * @class Express
 * @brief Represents the P4 Worker responsible for VIP batch deliveries.
 *
 * The Express class implements the logic for "Signal 2" operations.
 * When triggered, it attempts to load a "batch" (multiple packages) directly
 * onto the truck currently at the dock.
 *
 * Key features:
 * - **Priority Access:** Acquires the Dock Mutex directly, potentially blocking
 * the Dispatcher.
 * - **Bypasses Belt:** Does not interact with belt semaphores or limits ($K,
 * M$).
 * - **Batch Processing:** Generates and loads 3 to 5 packages in a single
 * operation.
 * - **Limit Checking:** Verifies Truck Weight ($W$) and Volume ($V$) limits per
 * package.
 */
class Express {
private:
  /** @brief Pointer to the system's Shared Memory segment. */
  SharedState *shm;

  /** @name Dependency Injection Callbacks
   * Functions injected by the Manager to allow interaction with IPC resources
   * without direct coupling to the Manager class.
   * @{ */
  std::function<void()> lock_dock_fn; /**< Callback to lock the Dock Mutex. */
  std::function<void()>
      unlock_dock_fn; /**< Callback to unlock the Dock Mutex. */
  std::function<void(pid_t, SignalType)>
      send_signal_fn; /**< Callback to send IPC signals. */
                      /** @} */

public:
  /**
   * @brief Constructs the Express worker logic controller.
   *
   * @param s Pointer to the Shared Memory state.
   * @param lock_dock Function to acquire exclusive access to the dock.
   * @param unlock_dock Function to release exclusive access to the dock.
   * @param send_signal Function to send control signals (e.g., DEPARTURE) to
   * other processes.
   */
  Express(SharedState *s, std::function<void()> lock_dock,
          std::function<void()> unlock_dock,
          std::function<void(pid_t, SignalType)> send_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

  /**
   * @brief Executes the delivery of a VIP package batch.
   *
   * This method contains the core business logic for P4:
   * 1. **Locking:** Acquires the dock mutex to ensure thread safety against the
   * Dispatcher.
   * 2. **Validation:** Checks if a truck is actually present.
   * 3. **Batch Generation:** Randomly determines a batch size (3-5 items).
   * 4. **Loading Loop:**
   * - Generates a package with random Weight (1-15kg) and Type (A, B, or C).
   * - Checks against Truck Capacity ($W$ and $V$).
   * - **If fits:** Updates shared memory stats and logs success.
   * - **If full:** Logs a warning, sends `SIGNAL_DEPARTURE` to the truck, and
   * aborts the rest of the batch.
   * 5. **Unlocking:** Releases the dock mutex.
   */
  void deliverExpressBatch() {
    if (!shm)
      return;

    lock_dock_fn();

    TruckState &truck = shm->dock_truck;

    if (!truck.is_present) {
      spdlog::warn("[P4] Cannot deliver Express - No truck at dock!");
      unlock_dock_fn();
      return;
    }

    spdlog::info("[P4] Delivering EXPRESS BATCH (Priority Order)!");

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> batch_dist(3, 5);
    int batch_size = batch_dist(gen);

    std::uniform_int_distribution<> type_dist(0, 2);
    std::uniform_real_distribution<> weight_dist(1.0, 15.0);

    for (int i = 0; i < batch_size; ++i) {
      double weight = weight_dist(gen);

      double vol = VOL_A;
      PackageType type = PackageType::TypeA;

      int t = type_dist(gen);
      if (t == 1) {
        vol = VOL_B;
        type = PackageType::TypeB;
      }
      if (t == 2) {
        vol = VOL_C;
        type = PackageType::TypeC;
      }

      bool fits_W = (truck.current_weight + weight <= truck.max_weight);
      bool fits_V = (truck.current_volume + vol <= truck.max_volume);

      if (fits_W && fits_V) {
        truck.current_weight += weight;
        truck.current_volume += vol;

        spdlog::info("[P4] Express Item {}/{} loaded (Type {}, {:.1f}kg). "
                     "Truck: {:.1f}% W, {:.1f}% V",
                     i + 1, batch_size, (int)type, weight,
                     (truck.current_weight / truck.max_weight) * 100,
                     (truck.current_volume / truck.max_volume) * 100);
      } else {
        spdlog::warn("[P4] Truck FULL during Express load! Batch incomplete. "
                     "Signaling Departure.");
        send_signal_fn(truck.id, SIGNAL_DEPARTURE);
        break;
      }
    }

    unlock_dock_fn();
  }
};
