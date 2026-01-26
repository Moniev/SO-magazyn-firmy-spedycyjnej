/**
 * @file Belt.h
 * @brief Logic for the conveyor belt system with Worker management.
 */
#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

class Belt {
private:
  SharedState *shm;

  std::function<void()> wait_empty_fn;
  std::function<void()> signal_empty_fn;
  std::function<void()> wait_full_fn;
  std::function<void()> signal_full_fn;
  std::function<void()> lock_fn;
  std::function<void()> unlock_fn;

  void simulateWorkLoad() {
    if (!shm)
      return;

    if (shm->current_workers_count <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return;
    }

    int delay = 500 / shm->current_workers_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }

public:
  Belt(SharedState *shared_state, std::function<void()> wait_empty,
       std::function<void()> signal_empty, std::function<void()> wait_full,
       std::function<void()> signal_full, std::function<void()> lock,
       std::function<void()> unlock)
      : shm(shared_state), wait_empty_fn(wait_empty),
        signal_empty_fn(signal_empty), wait_full_fn(wait_full),
        signal_full_fn(signal_full), lock_fn(lock), unlock_fn(unlock) {}

  bool registerWorker() {
    if (!shm)
      return false;
    bool success = false;
    lock_fn();
    if (shm->current_workers_count < MAX_WORKERS_PER_BELT) {
      shm->current_workers_count++;
      spdlog::info("[belt] Worker joined. Total: {}/{}",
                   shm->current_workers_count, MAX_WORKERS_PER_BELT);
      success = true;
    }
    unlock_fn();
    return success;
  }

  void unregisterWorker() {
    if (!shm)
      return;
    lock_fn();
    if (shm->current_workers_count > 0) {
      shm->current_workers_count--;
      spdlog::info("[belt] Worker left. Total: {}/{}",
                   shm->current_workers_count, MAX_WORKERS_PER_BELT);
    }
    unlock_fn();
  }

  void push(Package &pkg) {
    if (!shm)
      return;

    simulateWorkLoad();

    wait_empty_fn();
    lock_fn();

    if (shm->current_items_count >= MAX_BELT_CAPACITY_K) {
      spdlog::error("[belt] REJECTED: Belt full! Count: {}/{}",
                    shm->current_items_count, MAX_BELT_CAPACITY_K);

      unlock_fn();
      signal_empty_fn();
      return;
    }

    shm->total_packages_created++;
    pkg.id = shm->total_packages_created;

    int current_tail = shm->tail;
    shm->belt[current_tail] = pkg;

    shm->tail = (current_tail + 1) % MAX_BELT_CAPACITY_K;
    shm->current_items_count++;
    shm->current_belt_weight += pkg.weight;

    spdlog::info("[belt] Pushed ID {} at {}. Load: {}/{} (Workers: {})", pkg.id,
                 current_tail, shm->current_items_count, MAX_BELT_CAPACITY_K,
                 shm->current_workers_count);

    unlock_fn();
    signal_full_fn();
  }

  Package pop() {
    if (!shm)
      return {};
    wait_full_fn();
    lock_fn();

    if (shm->current_items_count <= 0) {
      unlock_fn();
      signal_full_fn();
      return {};
    }

    int current_head = shm->head;
    Package pkg = shm->belt[current_head];
    std::memset(&shm->belt[current_head], 0, sizeof(Package));
    shm->head = (current_head + 1) % MAX_BELT_CAPACITY_K;
    shm->current_items_count--;
    shm->current_belt_weight -= pkg.weight;

    spdlog::info("[belt] Popped ID {} from {}. Load: {}/{} (Workers: {})",
                 pkg.id, current_head, shm->current_items_count,
                 MAX_BELT_CAPACITY_K, shm->current_workers_count);

    unlock_fn();
    signal_empty_fn();
    return pkg;
  }

  int getCount() const { return shm ? shm->current_items_count : 0; }

  int getWorkerCount() const { return shm ? shm->current_workers_count : 0; }
};
