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
    if (pkg.id == 0)
      return;

    bool loaded = false;
    while (!loaded && shm && shm->running) {
      lock_dock_fn();
      TruckState &truck = shm->dock_truck;

      if (truck.is_present) {
        bool fits_qty = truck.current_load < truck.max_load;
        bool fits_wgt = (truck.current_weight + pkg.weight) <= truck.max_weight;

        if (fits_qty && fits_wgt) {
          truck.current_load++;
          truck.current_weight += pkg.weight;
          loaded = true;

          spdlog::info("[dispatcher] Test/Run: Pkg {} loaded.", pkg.id);

          if (truck.current_load >= truck.max_load ||
              truck.current_weight >= truck.max_weight) {
            send_signal_fn(truck.id, SIGNAL_DEPARTURE);
          }
        } else {
          if (truck.current_load > 0) {
            send_signal_fn(truck.id, SIGNAL_DEPARTURE);
          }
        }
      }
      unlock_dock_fn();

      if (!loaded) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  }

  void run() {
    spdlog::info("[dispatcher] Service started.");
    while (shm && shm->running) {
      processNextPackage();
      if (!shm->running)
        break;
    }
  }
};
