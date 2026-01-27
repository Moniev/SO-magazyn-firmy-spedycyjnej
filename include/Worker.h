/**
 * @file Worker.h
 * @brief Represents a single warehouse worker process (P1, P2, P3).
 */
#pragma once

#include "Manager.h"
#include <chrono>
#include <random>
#include <thread>

class Worker {
private:
  Manager *manager;
  bool active;
  int worker_id;

public:
  Worker(Manager *mgr, int id) : manager(mgr), active(true), worker_id(id) {}

  void run() {
    if (!manager->belt->registerWorker()) {
      spdlog::error("[worker-{}] Failed to register (Belt full of workers!)",
                    worker_id);
      return;
    }

    spdlog::info("[worker-{}] Started shift. Generating packages (A/B/C).",
                 worker_id);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> type_dist(0, 2);

    std::uniform_real_distribution<> weight_A(0.1, 8.0);
    std::uniform_real_distribution<> weight_B(8.0, 16.0);
    std::uniform_real_distribution<> weight_C(16.0, 25.0);

    while (active && manager->getState()->running) {
      if (manager->session_store->trySpawnProcess()) {

        Package p;
        p.creator_pid = getpid();
        p.status = PackageStatus::Normal;

        int type_roll = type_dist(gen);

        switch (type_roll) {
        case 0:
          p.type = PackageType::TypeA;
          p.volume = VOL_A;
          p.weight = weight_A(gen);
          break;
        case 1:
          p.type = PackageType::TypeB;
          p.volume = VOL_B;
          p.weight = weight_B(gen);
          break;
        case 2:
          p.type = PackageType::TypeC;
          p.volume = VOL_C;
          p.weight = weight_C(gen);
          break;
        }

        manager->belt->push(p);
        manager->session_store->reportProcessFinished();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
    }

    manager->belt->unregisterWorker();
    spdlog::info("[worker-{}] Shift ended.", worker_id);
  }

  void stop() { active = false; }
};
