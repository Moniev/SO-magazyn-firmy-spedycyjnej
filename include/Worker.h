/**
 * @file Worker.h
 * @brief Represents a single warehouse worker process.
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

    spdlog::info("[worker-{}] Started shift. Ready to load packages.",
                 worker_id);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> weight_dist(1.0, 15.0);

    while (active && manager->getState()->running) {
      if (manager->session_store->trySpawnProcess()) {

        Package p;
        p.creator_pid = getpid();
        p.weight = weight_dist(gen);
        p.type = (p.weight > 10.0) ? PackageType::TypeC : PackageType::TypeA;

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
