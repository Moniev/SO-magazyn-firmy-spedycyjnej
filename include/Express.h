/**
 * @file Express.h
 * @brief High-priority VIP package delivery logic.
 */

#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

/**
 * @class Express
 * @brief Manages "Express" or "VIP" package logistics.
 * * The Express class implements a priority bypass mechanism. Unlike standard
 * packages that must travel through the circular buffer (Belt), Express
 * packages are generated and loaded directly into the Truck at the dock.
 * * This class handles cross-mutex synchronization, as it must lock both the
 * Belt metrics (to generate a unique ID) and the Dock metrics (to perform the
 * load).
 */
class Express {
private:
  SharedState *shm; /**< Pointer to the shared memory state. */

  /** @name Synchronization and Messaging Hooks
   * @{ */
  std::function<void()> lock_dock_fn;   /**< Locks the Dock mutex. */
  std::function<void()> unlock_dock_fn; /**< Unlocks the Dock mutex. */
  std::function<void()>
      lock_belt_fn; /**< Locks the Belt mutex for ID generation. */
  std::function<void()> unlock_belt_fn; /**< Unlocks the Belt mutex. */
  std::function<void(SignalType)>
      send_signal_fn; /**< Dispatches signals to the Message Queue. */
  /** @} */

  /**
   * @brief Generates a random weight for the VIP package.
   * @return double Weight between 1.0 and 5.0 units.
   */
  double getRandomWeight() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0, 5.0);
    return dis(gen);
  }

public:
  /**
   * @brief Constructs an Express handler with required IPC functional hooks.
   */
  Express(SharedState *s, std::function<void()> lock_dock,
          std::function<void()> unlock_dock, std::function<void()> lock_belt,
          std::function<void()> unlock_belt,
          std::function<void(SignalType)> send_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        lock_belt_fn(lock_belt), unlock_belt_fn(unlock_belt),
        send_signal_fn(send_signal) {}

  /**
   * @brief Executes a VIP delivery sequence.
   * * @process_flow
   * 1. **ID Generation**: Locks the belt to increment the global package
   * counter.
   * 2. **Package Creation**: Instantiates a Package with
   * `PackageStatus::Express`.
   * 3. **Dock Verification**: Locks the dock and checks if a truck is
   * available.
   * 4. **Direct Loading**: If the truck has capacity (count and weight), loads
   * the package immediately.
   * 5. **Signaling**: If the VIP package fills the truck, it triggers an
   * immediate `SIGNAL_DEPARTURE`.
   * * @note If no truck is present or the truck is full, the VIP package is
   * effectively rejected to prevent the Express process from blocking the
   * entire system.
   */
  void deliverVipPackage() {
    if (!shm)
      return;

    spdlog::info("[express] Received VIP Order! Preparing package.");

    lock_belt_fn();
    shm->total_packages_created++;
    int new_id = shm->total_packages_created;
    unlock_belt_fn();

    Package pkg;
    pkg.id = new_id;
    pkg.weight = getRandomWeight();
    pkg.type = PackageType::TypeC;
    pkg.status = PackageStatus::Express;
    pkg.creator_pid = getpid();

    if (pkg.history_count < MAX_PACKAGE_HISTORY) {
      pkg.history[pkg.history_count] = {
          (ActionType::Created | ActionType::ByExpress), getpid(),
          std::time(nullptr)};
      pkg.history_count++;
    }

    lock_dock_fn();

    if (!shm->dock_truck.is_present) {
      spdlog::warn("[express] No truck for VIP Pkg #{}. Mission failed.",
                   pkg.id);
      unlock_dock_fn();
      return;
    }

    TruckState &truck = shm->dock_truck;

    bool fit_qty = truck.current_load < truck.max_load;
    bool fit_wgt = (truck.current_weight + pkg.weight) <= truck.max_weight;

    if (fit_qty && fit_wgt) {
      truck.current_load++;
      truck.current_weight += pkg.weight;

      spdlog::info("[express] >>> VIP DELIVERY <<< Pkg #{} loaded directly to "
                   "Truck #{}. (Load: {}/{})",
                   pkg.id, truck.id, truck.current_load, truck.max_load);

      if (truck.current_load >= truck.max_load) {
        spdlog::info(
            "[express] VIP Package filled the truck. Signaling departure.");
        send_signal_fn(SIGNAL_DEPARTURE);
      }

    } else {
      spdlog::warn("[express] Truck #{} is FULL! VIP Pkg #{} rejected. "
                   "Signaling departure.",
                   truck.id, pkg.id);
      send_signal_fn(SIGNAL_DEPARTURE);
    }

    unlock_dock_fn();
  }
};
