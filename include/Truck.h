#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

class Truck {
private:
  SharedState *shm;

  std::function<void()> lock_dock_fn;
  std::function<void()> unlock_dock_fn;
  std::function<SignalType()> wait_for_signal_fn;

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
  Truck(SharedState *s, std::function<void()> lock_dock,
        std::function<void()> unlock_dock,
        std::function<SignalType()> wait_for_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        wait_for_signal_fn(wait_for_signal) {}

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
