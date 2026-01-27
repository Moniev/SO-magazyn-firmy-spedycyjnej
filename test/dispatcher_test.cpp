/**
 * @file dispatcher_test.cpp
 * @brief Integration tests for the Dispatcher's package routing logic.
 * * These tests focus on the interaction between the Shared Memory segments,
 * checking that data transferred from the Belt buffer correctly manifests
 * in the TruckState structure.
 */

#include "../include/Manager.h"
#include <gtest/gtest.h>

/**
 * @class DispatcherTest
 * @brief Test fixture for verifying inter-process routing logic.
 * * The fixture ensures a clean slate by explicitly removing existing Linux IPC
 * resources (Shared Memory, Semaphores, Message Queues) before each test to
 * prevent cross-test interference or stale data corruption.
 */
class DispatcherTest : public ::testing::Test {
protected:
  /**
   * @brief Performs hard reset of System V IPC resources.
   * * Uses IPC_RMID to mark resources for destruction. A short sleep is
   * included to ensure the kernel has fully released the descriptors before the
   * next Manager instance attempts initialization.
   */
  void SetUp() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0), IPC_RMID, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
};

/**
 * @test SuccessfulLoad
 * @brief Verifies a standard end-to-end transfer.
 * * Scenario:
 * 1. Initialize IPC resources as the owner.
 * 2. Manually place a Truck in the dock via SharedState.
 * 3. Push a known Package onto the Belt.
 * 4. Invoke processNextPackage().
 * * Expected Result:
 * - Truck's current_load increments to 1.
 * - Truck's current_weight matches the Package weight.
 * - Shared memory integrity is maintained through Mutex locks.
 */
TEST_F(DispatcherTest, SuccessfulLoad) {
  Manager m(true);
  ASSERT_NE(m.getState(), nullptr) << "Shared memory not attached!";

  m.lockDock();

  TruckState &truck = m.getState()->dock_truck;
  truck.is_present = true;
  truck.id = 101;

  truck.max_load = 100;
  truck.max_weight = 100.0;
  truck.max_volume = 10.0;

  truck.current_load = 0;
  truck.current_weight = 0.0;
  truck.current_volume = 0.0;

  m.unlockDock();

  Package p;
  p.id = 500;
  p.weight = 10.5;
  p.volume = 0.1;

  m.belt->push(p);

  m.dispatcher->processNextPackage();

  m.lockDock();
  EXPECT_EQ(m.getState()->dock_truck.current_load, 1);
  EXPECT_DOUBLE_EQ(m.getState()->dock_truck.current_weight, 10.5);
  EXPECT_DOUBLE_EQ(m.getState()->dock_truck.current_volume, 0.1);
  m.unlockDock();
}
