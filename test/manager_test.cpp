#include "../include/Manager.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

class ManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0666), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0666), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0666), IPC_RMID, nullptr);
  }

  void TearDown() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0666), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0666), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0666), IPC_RMID, nullptr);
  }
};

TEST_F(ManagerTest, InitializationAsOwner) {
  ASSERT_NO_THROW({
    Manager manager(true); // Owner

    SharedState *state = manager.getState();
    ASSERT_NE(state, nullptr);

    EXPECT_TRUE(state->running);
    EXPECT_EQ(state->trucks_completed, 0);
    EXPECT_EQ(state->total_packages_created, 0);
  });
}

TEST_F(ManagerTest, SharedMemorySync) {
  Manager owner(true);
  owner.getState()->current_belt_weight = 12.5;
  owner.getState()->head = 5;

  Manager client(false);

  EXPECT_DOUBLE_EQ(client.getState()->current_belt_weight, 12.5);
  EXPECT_EQ(client.getState()->head, 5);

  client.getState()->tail = 3;
  EXPECT_EQ(owner.getState()->tail, 3);
}

TEST_F(ManagerTest, MessageQueueCommunication) {
  Manager owner(true);
  Manager client(false);

  EXPECT_EQ(client.receiveSignalNonBlocking(), SIGNAL_NONE);

  owner.sendSignal(SIGNAL_DEPARTURE);

  SignalType received = client.receiveSignalNonBlocking();
  EXPECT_EQ(received, SIGNAL_DEPARTURE);

  EXPECT_EQ(client.receiveSignalNonBlocking(), SIGNAL_NONE);
}

TEST_F(ManagerTest, SemaphoreSanity) {
  Manager manager(true);

  ASSERT_NO_THROW(manager.lockBelt());
  manager.getState()->current_items_count++;
  ASSERT_NO_THROW(manager.unlockBelt());

  ASSERT_NO_THROW(manager.lockDock());
  ASSERT_NO_THROW(manager.unlockDock());
}

TEST_F(ManagerTest, SemaphoreBlockingLogic) {
  Manager owner(true);

  bool criticalSectionVisited = false;

  owner.lockBelt();

  std::thread worker([&]() {
    Manager client(false);
    client.lockBelt();

    criticalSectionVisited = true;

    client.unlockBelt();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(criticalSectionVisited);

  owner.unlockBelt();

  worker.join();
  EXPECT_TRUE(criticalSectionVisited);
}
