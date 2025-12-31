#pragma once

#include "Shared.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <functional>
#include <optional>

class Belt {
private:
  SharedState *shm;

  std::function<void()> wait_empty_fn;
  std::function<void()> signal_empty_fn;

  std::function<void()> wait_full_fn;
  std::function<void()> signal_full_fn;

  std::function<void()> lock_fn;
  std::function<void()> unlock_fn;

public:
  Belt(SharedState *shared_state, std::function<void()> wait_empty,
       std::function<void()> signal_empty, std::function<void()> wait_full,
       std::function<void()> signal_full, std::function<void()> lock,
       std::function<void()> unlock)
      : shm(shared_state), wait_empty_fn(wait_empty),
        signal_empty_fn(signal_empty), wait_full_fn(wait_full),
        signal_full_fn(signal_full), lock_fn(lock), unlock_fn(unlock) {}

  void push(Package &pkg) {
    if (!shm)
      return;

    wait_empty_fn();
    lock_fn();

    int current_tail = shm->tail;
    shm->belt[current_tail] = pkg;

    shm->tail = (current_tail + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count++;
    shm->current_belt_weight += pkg.weight;
    shm->total_packages_created++;

    spdlog::info("[belt] Pushed ID {} at {}. Load: {}/{}", pkg.id, current_tail,
                 shm->current_items_count, MAX_BELT_CAPACITY_K);

    unlock_fn();

    signal_full_fn();
  }

  Package pop() {
    if (!shm)
      return {};

    wait_full_fn();

    lock_fn();

    int current_head = shm->head;
    Package pkg = shm->belt[current_head];

    std::memset(&shm->belt[current_head], 0, sizeof(Package));

    shm->head = (current_head + 1) % MAX_BELT_CAPACITY_K;

    shm->current_items_count--;
    shm->current_belt_weight -= pkg.weight;

    spdlog::info("[belt] Popped ID {} from {}. Load: {}/{}", pkg.id,
                 current_head, shm->current_items_count, MAX_BELT_CAPACITY_K);

    unlock_fn();
    signal_empty_fn();

    return pkg;
  }

  int getCount() const { return shm ? shm->current_items_count : 0; }
};
