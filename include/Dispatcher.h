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
  std::function<void(pid_t, SignalType)> send_signal_fn;

public:
  Dispatcher(Belt *b, SharedState *s, std::function<void()> lock_dock,
             std::function<void()> unlock_dock,
             std::function<void(pid_t, SignalType)> send_signal)
      : belt(b), shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

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

          if (truck.current_weight >= truck.max_weight * 0.99 ||
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

  void run() {
    spdlog::info("[dispatcher] Service started. Controlling the dock.");

    while (shm && shm->running) {
      processNextPackage();
    }

    spdlog::info("[dispatcher] Service stopped.");
  }
};
