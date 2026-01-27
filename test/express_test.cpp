/**
 * @file express_test.cpp
 * @brief Unit tests for the Express (VIP) delivery logic.
 * * These tests utilize a mock-callback strategy to verify that the Express
 * class:
 * 1. Corrects interacts with Belt and Dock mutexes.
 * 2. Properly increments system-wide package IDs.
 * 3. Corrects calculates truck load/weight limits.
 * 4. Triggers the DEPARTURE signal via the Message Queue when appropriate.
 */

#include "../include/Express.h"
#include <cstring>
#include <gtest/gtest.h>

/**
 * @class ExpressTest
 * @brief Test fixture for mocking IPC behaviors.
 * * Instead of using real semaphores, this fixture tracks "locks" and "signals"
 * using counters to verify that the Express class follows the correct
 * synchronization protocol.
 */
class ExpressTest : public ::testing::Test {
protected:
  /** @brief Local stack-allocated shared state acting as a memory sandbox. */
  SharedState mock_shared_memory;

  /** @name Synchronization Trackers
   * Counters used to verify that the Express class correctly acquires and
   * releases system-wide mutexes.
   * @{ */
  int dock_locks = 0; /**< Tracks how many times the Dock Mutex was acquired. */
  int belt_locks = 0; /**< Tracks how many times the Belt Mutex was acquired. */
  /** @} */

  /** @name Signaling Trackers
   * State variables used to intercept and verify inter-process signals.
   * @{ */
  int signals_sent =
      0; /**< Total number of signals dispatched via the mock queue. */
  SignalType last_signal =
      SIGNAL_NONE; /**< Captures the last SignalType sent for inspection. */
  pid_t last_target_pid = 0; /**< Captures the pid of last signal */
  /** @} */

  /** @brief Mock implementation of the Dock lock; increments the local tracker.
   */
  std::function<void()> mock_dock_lock = [this]() { dock_locks++; };

  /** @brief Mock implementation of the Dock unlock; currently a no-op. */
  std::function<void()> mock_dock_unlock = []() {};

  /** @brief Mock implementation of the Belt lock; increments the local tracker.
   */
  std::function<void()> mock_belt_lock = [this]() { belt_locks++; };

  /** @brief Mock implementation of the Belt unlock; currently a no-op. */
  std::function<void()> mock_belt_unlock = []() {};

  /** * @brief Mock implementation of the signal dispatcher.
   * Captures the signal type into last_signal for verification in ASSERT/EXPECT
   * macros.
   */
  std::function<void(pid_t, SignalType)> mock_signal = [this](pid_t p,
                                                              SignalType s) {
    signals_sent++;
    last_target_pid = p;
    last_signal = s;
  };

  /**
   * @brief Prepares the mock environment for VIP delivery simulation.
   * * Initializes the mock memory segment and places a virtual Truck in the
   * dock with predefined capacity and weight limits to simulate a "Ready"
   * state.
   */
  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
    mock_shared_memory.dock_truck.is_present = true;
    mock_shared_memory.dock_truck.max_load = 10;
    mock_shared_memory.dock_truck.max_weight = 100.0;
    mock_shared_memory.dock_truck.id = 99;
  }
};

/**
 * @test DeliverVipSuccessfully
 * @brief Validates the standard VIP loading path.
 * - Verifies that both Belt and Dock mutexes were accessed.
 * - Confirms that total_packages_created and truck current_load were
 * incremented.
 */
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

/**
 * @test HandleNoTruckGracefully
 * @brief Ensures the system does not crash if a VIP order arrives at an empty
 * dock.
 * - The ID should still be generated, but the truck state must remain
 * unchanged.
 */
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

/**
 * @test VipFillsTruckAndSignalsDeparture
 * @brief Verifies that the Express process can trigger a truck departure.
 * - If the VIP package is the last item to fit, a SIGNAL_DEPARTURE must be
 * sent.
 */
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

/**
 * @test TruckFullVipRejectedAndSignals
 * @brief Verifies behavior when a VIP package arrives at a full truck.
 * - Package is rejected, and a departure signal is sent to clear the dock.
 */
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
