#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

class Express {
private:
  SharedState *shm;

  std::function<void()> lock_dock_fn;
  std::function<void()> unlock_dock_fn;
  std::function<void()> lock_belt_fn;
  std::function<void()> unlock_belt_fn;
  std::function<void(pid_t, SignalType)> send_signal_fn;

  double getRandomWeight() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0, 5.0);
    return dis(gen);
  }

public:
  Express(SharedState *s, std::function<void()> lock_dock,
          std::function<void()> unlock_dock, std::function<void()> lock_belt,
          std::function<void()> unlock_belt,
          std::function<void(pid_t, SignalType)> send_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        lock_belt_fn(lock_belt), unlock_belt_fn(unlock_belt),
        send_signal_fn(send_signal) {}

  void deliverVipPackage() {
    if (!shm)
      return;

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

    spdlog::info("[express] VIP Order #{} created! Attempting priority load...",
                 pkg.id);

    lock_dock_fn();

    if (!shm->dock_truck.is_present) {
      spdlog::warn("[express] No truck available for VIP #{}. Order canceled.",
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

      spdlog::info(
          "[express] Loaded VIP #{} -> Truck #{} (PID: {}). Load: {}/{}",
          pkg.id, truck.id, truck.id, truck.current_load, truck.max_load);

      if (truck.current_load >= truck.max_load ||
          truck.current_weight >= truck.max_weight) {
        spdlog::info("[express] Truck #{} filled by VIP. Signaling departure.",
                     truck.id);
        send_signal_fn(truck.id, SIGNAL_DEPARTURE);
      }

    } else {
      spdlog::warn(
          "[express] Truck #{} FULL! VIP #{} forcing departure to clear dock.",
          truck.id, pkg.id);
      send_signal_fn(truck.id, SIGNAL_DEPARTURE);
    }

    unlock_dock_fn();
  }
};
