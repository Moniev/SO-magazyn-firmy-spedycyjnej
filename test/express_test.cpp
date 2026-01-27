#include "../include/Express.h"
#include <cstring>
#include <gtest/gtest.h>

class ExpressTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;
  int dock_locks = 0;
  int signals_sent = 0;
  SignalType last_signal = SIGNAL_NONE;
  pid_t last_target_pid = 0;

  std::function<void()> mock_dock_lock = [this]() { dock_locks++; };
  std::function<void()> mock_dock_unlock = []() {};
  std::function<void(pid_t, SignalType)> mock_signal = [this](pid_t p,
                                                              SignalType s) {
    signals_sent++;
    last_target_pid = p;
    last_signal = s;
  };

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));

    TruckState &t = mock_shared_memory.dock_truck;
    t.is_present = true;
    t.id = 99;

    t.max_load = 100;
    t.max_weight = 1000.0;
    t.max_volume = 100.0;

    t.current_load = 0;
    t.current_weight = 0.0;
    t.current_volume = 0.0;
  }
};

TEST_F(ExpressTest, DeliverExpressBatch_Success) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_signal);

  express.deliverExpressBatch();

  EXPECT_GE(dock_locks, 1);
  EXPECT_GT(mock_shared_memory.dock_truck.current_weight, 0.0);
  EXPECT_GT(mock_shared_memory.dock_truck.current_volume, 0.0);
}

TEST_F(ExpressTest, HandleNoTruckGracefully) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_signal);

  mock_shared_memory.dock_truck.is_present = false;

  express.deliverExpressBatch();

  EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 0);
  EXPECT_EQ(signals_sent, 0);
}

TEST_F(ExpressTest, TruckFull_TriggersDeparture) {
  Express express(&mock_shared_memory, mock_dock_lock, mock_dock_unlock,
                  mock_signal);

  mock_shared_memory.dock_truck.max_weight = 0.1;
  express.deliverExpressBatch();

  EXPECT_GE(signals_sent, 1);
  EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}
