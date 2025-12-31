#include "../include/Manager.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

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
    Manager manager(true);

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

  SignalType received_signal = client.receiveSignalNonBlocking();
  EXPECT_EQ(received_signal, SIGNAL_DEPARTURE);

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

  bool critical_section_visited = false;

  owner.lockBelt();

  std::thread worker([&]() {
    Manager client(false);
    client.lockBelt();

    critical_section_visited = true;

    client.unlockBelt();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(critical_section_visited);

  owner.unlockBelt();

  worker.join();
  EXPECT_TRUE(critical_section_visited);
}

TEST_F(ManagerTest, SessionManager_BasicLifecycle) {
  Manager mgr(true);

  ASSERT_TRUE(mgr.session_store->login("TestUser", 2));
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());
  EXPECT_FALSE(mgr.session_store->trySpawnProcess());

  mgr.session_store->reportProcessFinished();
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());

  mgr.session_store->logout();
  EXPECT_FALSE(mgr.session_store->trySpawnProcess());
}

TEST_F(ManagerTest, SessionManager_MultiUserIsolation) {
  Manager admin(true);
  Manager guest(false);

  ASSERT_TRUE(admin.session_store->login("Admin", 10));
  ASSERT_TRUE(guest.session_store->login("Guest", 1));

  EXPECT_TRUE(admin.session_store->trySpawnProcess());
  EXPECT_TRUE(guest.session_store->trySpawnProcess());

  EXPECT_FALSE(guest.session_store->trySpawnProcess());
  EXPECT_TRUE(admin.session_store->trySpawnProcess());
}

TEST_F(ManagerTest, SessionManager_PreventDuplicateLogin) {
  Manager mgr_1(true);
  Manager mgr_2(false);

  ASSERT_TRUE(mgr_1.session_store->login("Operator", 5));

  EXPECT_FALSE(mgr_2.session_store->login("Operator", 5));
  EXPECT_TRUE(mgr_2.session_store->login("OtherUser", 5));
}

TEST_F(ManagerTest, SessionManager_MaxSessionsLimit) {
  Manager mgr(true);

  std::vector<std::unique_ptr<Manager>> clients;

  for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
    auto client_mgr = std::make_unique<Manager>(false);
    std::string name = "User" + std::to_string(i);

    ASSERT_TRUE(client_mgr->session_store->login(name, 1));
    clients.push_back(std::move(client_mgr));
  }

  Manager overflow_client(false);
  EXPECT_FALSE(overflow_client.session_store->login("UserOverflow", 1));
}

TEST_F(ManagerTest, Belt_Integration_BasicPushPop) {
  Manager mgr(true);

  Package pkg_in;
  pkg_in.weight = 50.0;

  mgr.belt->push(pkg_in);

  SharedState *state = mgr.getState();
  EXPECT_EQ(state->current_items_count, 1);
  EXPECT_EQ(state->total_packages_created, 1);
  EXPECT_DOUBLE_EQ(state->current_belt_weight, 50.0);

  Package pkg_out = mgr.belt->pop();

  EXPECT_EQ(pkg_out.id, 1);
  EXPECT_DOUBLE_EQ(pkg_out.weight, 50.0);
  EXPECT_EQ(state->current_items_count, 0);
}

/*
 * Thread is being used because of necessity.
 * Kernel is not interested if semaphore blocks thread or process
 * so let it slide for thread in tests as this is just easier to run
 */
TEST_F(ManagerTest, Belt_Integration_BlockingConsumer) {
  Manager producer(true);

  std::atomic<bool> pop_finished{false};

  std::thread consumer_thread([&]() {
    Manager consumer(false);
    consumer.belt->pop();
    pop_finished = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(pop_finished);

  Package p;
  producer.belt->push(p);

  consumer_thread.join();
  EXPECT_TRUE(pop_finished);
}

TEST_F(ManagerTest, Belt_Integration_BlockingProducer) {
  Manager producer(true);

  for (int i = 0; i < MAX_BELT_CAPACITY_K; ++i) {
    Package p;
    producer.belt->push(p);
  }

  EXPECT_EQ(producer.getState()->current_items_count, MAX_BELT_CAPACITY_K);

  std::atomic<bool> push_finished{false};

  std::thread producer_thread([&]() {
    Manager threaded_producer(false);
    Package overflow_pkg;
    threaded_producer.belt->push(overflow_pkg);
    push_finished = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(push_finished);

  Manager consumer(false);
  consumer.belt->pop();

  producer_thread.join();
  EXPECT_TRUE(push_finished);
}
