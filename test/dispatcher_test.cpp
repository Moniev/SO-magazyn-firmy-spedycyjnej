#include "../include/Manager.h"
#include <gtest/gtest.h>

class DispatcherTest : public ::testing::Test {
protected:
  void SetUp() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0), IPC_RMID, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  void TearDown() override {}
};

TEST_F(DispatcherTest, SuccessfulLoad) {
  Manager m(true);
  ASSERT_NE(m.getState(), nullptr) << "Shared memory not attached!";

  m.lockDock();
  m.getState()->dock_truck.is_present = true;
  m.getState()->dock_truck.id = 101;
  m.getState()->dock_truck.max_load = 2;
  m.getState()->dock_truck.current_load = 0;
  m.getState()->dock_truck.current_weight = 0.0;
  m.unlockDock();

  Package p;
  p.id = 500;
  p.weight = 10.5;
  m.belt->push(p);

  m.dispatcher->processNextPackage();

  m.lockDock();
  EXPECT_EQ(m.getState()->dock_truck.current_load, 1);
  EXPECT_DOUBLE_EQ(m.getState()->dock_truck.current_weight, 10.5);
  m.unlockDock();
}
