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
    if (pkg.id == 0)
      return;

    bool loaded = false;

    while (!loaded && shm->running) {
      lock_dock_fn();

      if (!shm->dock_truck.is_present) {
        spdlog::warn("[dispatcher] No truck in dock! Pkg {} waiting...",
                     pkg.id);
        unlock_dock_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }

      TruckState &truck = shm->dock_truck;

      if (truck.current_load < truck.max_load) {
        truck.current_load++;
        truck.current_weight += pkg.weight;
        pkg.pushAction(ActionType::LoadedToTruck, getpid());

        spdlog::info("[dispatcher] Loaded Pkg {} into Truck {}. Load: {}/{}",
                     pkg.id, truck.id, truck.current_load, truck.max_load);

        loaded = true;

        if (truck.current_load >= truck.max_load) {
          spdlog::info(
              "[dispatcher] Truck {} reached capacity. Sending DEPARTURE.",
              truck.id);
          send_signal_fn(SIGNAL_DEPARTURE);
        }
      } else {
        spdlog::warn("[dispatcher] Truck {} is FULL. Signaling DEPARTURE again "
                     "and waiting for new truck for Pkg {}.",
                     truck.id, pkg.id);
        send_signal_fn(SIGNAL_DEPARTURE);
        unlock_dock_fn();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }

      unlock_dock_fn();
    }
  }
};
