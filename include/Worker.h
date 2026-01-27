/**
 * @file Worker.h
 * @brief Defines the Worker agent responsible for package generation.
 *
 * This file contains the implementation of the warehouse worker (P1, P2, P3).
 * The Worker acts as a Producer in the Producer-Consumer pattern, generating
 * packages with varying specifications (Type A, B, C) and placing them onto
 * the conveyor belt.
 */

#pragma once

#include "Manager.h"
#include <chrono>
#include <random>
#include <thread>

/**
 * @class Worker
 * @brief Represents a manual warehouse worker (e.g., P1, P2, P3).
 *
 * The Worker class simulates an employee stationed at the start of the conveyor
 * belt. Its primary responsibility is to continuously generate packages with
 * randomized characteristics (Dimensions and Weight) and attempt to place them
 * onto the shared conveyor belt.
 *
 * @note This class runs as a blocking loop within its own process context.
 */
class Worker {
private:
  /**
   * @brief Pointer to the central system orchestrator.
   * Provides access to shared resources like the Belt and Session Store.
   */
  Manager *manager;

  /**
   * @brief Control flag for the main execution loop.
   * If set to false, the worker will finish the current iteration and
   * terminate.
   */
  bool active;

  /**
   * @brief Numeric identifier for logging and identification purposes (e.g., 1,
   * 2, 3).
   */
  int worker_id;

public:
  /**
   * @brief Constructs a new Worker instance.
   *
   * @param mgr Pointer to the initialized Manager instance providing IPC
   * access.
   * @param id The unique ID assigned to this worker process.
   */
  Worker(Manager *mgr, int id) : manager(mgr), active(true), worker_id(id) {}

  /**
   * @brief The main operational loop of the worker.
   *
   * This method performs the following lifecycle operations:
   * 1. **Registration:** Attempts to register with the Belt subsystem.
   * If the belt has too many active workers, this method returns immediately.
   * 2. **RNG Initialization:** Sets up Mersenne Twister engine for stochastic
   * generation.
   * 3. **Production Loop:**
   * - Checks session quotas via `trySpawnProcess()`.
   * - Generates a package type (A, B, or C) using a uniform distribution.
   * - Assigns weight based on the package type to simulate "smaller = lighter":
   * - **Type A:** 0.1 kg - 8.0 kg
   * - **Type B:** 8.0 kg - 16.0 kg
   * - **Type C:** 16.0 kg - 25.0 kg
   * - Pushes the package to the Belt (blocking if belt is full).
   * 4. **Cleanup:** Unregisters the worker upon loop termination.
   *
   * @warning This method blocks the calling thread until `stop()` is called
   * or the system global flag `running` becomes false.
   */
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

  /**
   * @brief Signals the worker to stop generating packages.
   *
   * Sets the internal `active` flag to false. The worker will exit the `run()`
   * loop after the current iteration completes.
   */
  void stop() { active = false; }
};
