#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>
#include <thread>
#include <unistd.h>

class Truck {
private:
  SharedState *shm;
  pid_t my_pid;

  std::function<void()> lock_dock_fn;
  std::function<void()> unlock_dock_fn;
  std::function<SignalType(pid_t)> wait_for_signal_fn;

  void randomizeTruckSpecs(TruckState &truck) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> load_dist(5, 12);
    std::uniform_real_distribution<> weight_dist(50.0, 120.0);

    truck.id = my_pid;

    truck.current_load = 0;
    truck.current_weight = 0.0;
    truck.max_load = load_dist(gen);
    truck.max_weight = weight_dist(gen);
    truck.is_present = true;
  }

public:
  Truck(SharedState *s, std::function<void()> lock_dock,
        std::function<void()> unlock_dock,
        std::function<SignalType(pid_t)> wait_for_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        wait_for_signal_fn(wait_for_signal) {
    my_pid = getpid();
  }

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
      spdlog::info("[truck-{}] Docked successfully. Waiting for cargo.",
                   my_pid);

      unlock_dock_fn();

      SignalType sig = wait_for_signal_fn(my_pid);

      if (sig == SIGNAL_END_WORK || !shm->running) {
        break;
      }

      lock_dock_fn();

      if (shm->dock_truck.id == my_pid) {
        shm->trucks_completed++;
        shm->dock_truck.is_present = false;
        spdlog::info("[truck-{}] Departing full. Total dispatched: {}", my_pid,
                     shm->trucks_completed);
      } else {
        spdlog::critical(
            "[truck-{}] ERROR: Identity theft at dock! Found ID {}", my_pid,
            shm->dock_truck.id);
      }

      unlock_dock_fn();

      int route_time = 3000 + (rand() % 5000);
      spdlog::info("[truck-{}] On route... returning in {}ms", my_pid,
                   route_time);
      std::this_thread::sleep_for(std::chrono::milliseconds(route_time));
    }

    spdlog::info("[truck-{}] Shift ended.", my_pid);
  }
};
