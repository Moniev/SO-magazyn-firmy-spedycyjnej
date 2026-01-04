#include "../include/Truck.h"
#include <cstring>
#include <gtest/gtest.h>
#include <queue>

class TruckTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;

  int lock_calls = 0;
  int unlock_calls = 0;

  std::queue<SignalType> signal_scenario;

  std::function<void()> mock_lock = [this]() { lock_calls++; };
  std::function<void()> mock_unlock = [this]() { unlock_calls++; };

  std::function<SignalType()> mock_wait = [this]() {
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

TEST_F(TruckTest, InitializesTruckDataInSharedMemory) {
  signal_scenario.push(SIGNAL_DEPARTURE);
  signal_scenario.push(SIGNAL_END_WORK);

  Truck truck(&mock_shared_memory, mock_lock, mock_unlock, mock_wait);
  truck.run();

  EXPECT_NE(mock_shared_memory.dock_truck.id, 0);
  EXPECT_GT(mock_shared_memory.dock_truck.max_load, 0);
}
