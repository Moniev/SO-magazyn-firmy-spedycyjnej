#pragma once

#include "Belt.h"
#include "Shared.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <functional>
#include <thread>

class Dispatcher {
private:
  Belt *belt;
  SharedState *shm;

  std::function<void()> lock_dock_fn;
  std::function<void()> unlock_dock_fn;
  std::function<void(SignalType)> send_signal_fn;

public:
  Dispatcher(Belt *b, SharedState *s, std::function<void()> lock_dock,
             std::function<void()> unlock_dock,
             std::function<void(SignalType)> send_signal)
      : belt(b), shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

  void processNextPackage() {
    if (!belt || !shm)
      return;

    Package pkg = belt->pop();

    if (pkg.id == 0) {
      spdlog::warn("[dispatcher] Received invalid package ID 0.");
      return;
    }

    lock_dock_fn();

    if (!shm->dock_truck.is_present) {
      spdlog::warn("[dispatcher] No truck in dock! Holding package {}...",
                   pkg.id);

      unlock_dock_fn();
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      spdlog::error("[dispatcher] Package {} put aside (waiting for truck).",
                    pkg.id);
      return;
    }

    TruckState &truck = shm->dock_truck;

    bool fit_quantity = truck.current_load < truck.max_load;
    bool fit_weight = (truck.current_weight + pkg.weight) <= truck.max_weight;

    if (fit_quantity && fit_weight) {
      truck.current_load++;
      truck.current_weight += pkg.weight;

      pkg.pushAction(ActionType::LoadedToTruck, getpid());

      spdlog::info(
          "[dispatcher] Loaded Pkg #{} ({:.1f}kg) -> Truck #{}. Load: {}/{}",
          pkg.id, pkg.weight, truck.id, truck.current_load, truck.max_load);
    } else {
      spdlog::warn("[dispatcher] Truck #{} FULL! Cannot load Pkg #{}. Calling "
                   "departure.",
                   truck.id, pkg.id);
      send_signal_fn(SIGNAL_DEPARTURE);
    }

    if (truck.current_load >= truck.max_load ||
        truck.current_weight >= truck.max_weight) {
      spdlog::info(
          "[dispatcher] Truck #{} capacity reached. Signaling departure!",
          truck.id);
      send_signal_fn(SIGNAL_DEPARTURE);
    }

    unlock_dock_fn();
  }
};
