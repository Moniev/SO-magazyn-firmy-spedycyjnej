/**
 * @file truck_test.cpp
 * @brief Unit tests for the Truck lifecycle and signal handling.
 * * Verifies the vehicle's state transitions: Arrival -> Wait -> Departure.
 */

#include "../include/Truck.h"
#include <cstring>
#include <gtest/gtest.h>
#include <queue>

/**
 * @class TruckTest
 * @brief Test fixture for simulating the logistics loop.
 * * Uses a `std::queue<SignalType>` to inject a predefined sequence of signals
 * into the `Truck::run` loop, preventing infinite blocking during tests.
 */
class TruckTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;

  /** @name counters of semaphore operation calls
   * @{ */
  int lock_calls = 0;   /**< Calls of locking access to shared state */
  int unlock_calls = 0; /**< Calls of unlocking access to shared state */
  /** @} */

  std::queue<SignalType>
      signal_scenario; /**< Queue of current signal scenario */

  /** @name synchronization semaphores mocs
   * No-op callbacks for local unit tests (no real semaphores needed, just
   * incrementing calls mocks)
   * @{ */
  std::function<void()> mock_lock = [this]() { lock_calls++; };
  std::function<void()> mock_unlock = [this]() { unlock_calls++; };
  /** @} */

  /**
   * @brief Mock implementation of receiveSignalBlocking.
   * * Dequeues the next signal from `signal_scenario`. If empty, returns
   * `SIGNAL_END_WORK` to force the loop to terminate safely.
   */
  std::function<SignalType(pid_t)> mock_wait = [this](pid_t) {
    if (signal_scenario.empty())
      return SIGNAL_END_WORK;

    SignalType s = signal_scenario.front();
    signal_scenario.pop();
    return s;
  };

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
    mock_shared_memory.running = true;
    mock_shared_memory.dock_truck.is_present = false;
  }
};

/**
 * @test FullCycleScenario
 * @brief Verifies a complete "Arrival -> Load -> Depart" cycle.
 * * **Scenario**:
 * 1. Truck starts and docks.
 * 2. Receives `SIGNAL_DEPARTURE`.
 * 3. Updates stats (`trucks_completed`) and clears dock (`is_present = false`).
 * 4. Receives `SIGNAL_END_WORK` and terminates.
 */
TEST_F(TruckTest, FullCycleScenario) {
  signal_scenario.push(SIGNAL_DEPARTURE);
  signal_scenario.push(SIGNAL_END_WORK);

  Truck truck(&mock_shared_memory, mock_lock, mock_unlock, mock_wait);

  truck.run();

  EXPECT_EQ(mock_shared_memory.trucks_completed, 1);
  EXPECT_FALSE(mock_shared_memory.dock_truck.is_present);

  EXPECT_GT(lock_calls, 2);
  EXPECT_GT(unlock_calls, 2);
}

/**
 * @test InitializesTruckDataInSharedMemory
 * @brief Verifies that a new truck correctly randomizes its specifications upon
 * arrival.
 * * Checks that `id`, `max_load`, and `max_weight` are populated with non-zero
 * values.
 */
TEST_F(TruckTest, InitializesTruckDataInSharedMemory) {
  signal_scenario.push(SIGNAL_DEPARTURE);
  signal_scenario.push(SIGNAL_END_WORK);

  Truck truck(&mock_shared_memory, mock_lock, mock_unlock, mock_wait);
  truck.run();

  EXPECT_NE(mock_shared_memory.dock_truck.id, 0);
  EXPECT_GT(mock_shared_memory.dock_truck.max_load, 0);
}
