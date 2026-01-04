#include "../include/Express.h"
#include <cstring>
#include <gtest/gtest.h>

class ExpressTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;

  int dock_locks = 0;
  int belt_locks = 0;
  int signals_sent = 0;
  SignalType last_signal = SIGNAL_NONE;

  std::function<void()> mock_dock_lock = [this]() { dock_locks++; };
  std::function<void()> mock_dock_unlock = []() {};
  std::function<void()> mock_belt_lock = [this]() { belt_locks++; };
  std::function<void()> mock_belt_unlock = []() {};

  std::function<void(SignalType)> mock_signal = [this](SignalType s) {
    signals_sent++;
    last_signal = s;
  };

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
    mock_shared_memory.dock_truck.is_present = true;
    mock_shared_memory.dock_truck.max_load = 10;
    mock_shared_memory.dock_truck.max_weight = 100.0;
    mock_shared_memory.dock_truck.id = 99;
  }
};

TEST_F(ExpressTest, DeliverVipSuccessfully) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_belt_lock, mock_belt_unlock, mock_signal);

  mock_shared_memory.total_packages_created = 10;
  mock_shared_memory.dock_truck.current_load = 0;

  express.deliverVipPackage();

  EXPECT_EQ(mock_shared_memory.total_packages_created, 11);
  EXPECT_EQ(belt_locks, 1);
  EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 1);
  EXPECT_GT(mock_shared_memory.dock_truck.current_weight, 0.0);
  EXPECT_EQ(dock_locks, 1);
  EXPECT_EQ(signals_sent, 0);
}

TEST_F(ExpressTest, HandleNoTruckGracefully) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_belt_lock, mock_belt_unlock, mock_signal);

  mock_shared_memory.dock_truck.is_present = false;
  mock_shared_memory.total_packages_created = 50;

  express.deliverVipPackage();

  EXPECT_EQ(mock_shared_memory.total_packages_created, 51);
  EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 0);
  EXPECT_EQ(signals_sent, 0);
}

TEST_F(ExpressTest, VipFillsTruckAndSignalsDeparture) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_belt_lock, mock_belt_unlock, mock_signal);

  mock_shared_memory.dock_truck.current_load = 9;
  mock_shared_memory.dock_truck.max_load = 10;

  express.deliverVipPackage();

  EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 10);

  EXPECT_EQ(signals_sent, 1);
  EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}

TEST_F(ExpressTest, TruckFullVipRejectedAndSignals) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_belt_lock, mock_belt_unlock, mock_signal);

  mock_shared_memory.dock_truck.current_load = 10;
  mock_shared_memory.dock_truck.max_load = 10;

  express.deliverVipPackage();

  EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 10);
  EXPECT_EQ(signals_sent, 1);
  EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}
