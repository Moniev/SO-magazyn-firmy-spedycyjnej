#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <functional>
#include <random>

class ExpressWorker {
private:
  SharedState *shm;

  std::function<void()> lock_dock_fn;
  std::function<void()> unlock_dock_fn;
  std::function<void(pid_t, SignalType)> send_signal_fn;

public:
  ExpressWorker(SharedState *s, std::function<void()> lock_dock,
                std::function<void()> unlock_dock,
                std::function<void(pid_t, SignalType)> send_signal)
      : shm(s), lock_dock_fn(lock_dock), unlock_dock_fn(unlock_dock),
        send_signal_fn(send_signal) {}

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
