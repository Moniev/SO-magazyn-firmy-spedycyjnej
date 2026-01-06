/**
 * @file manager_test.cpp
 * @brief Comprehensive integration tests for the IPC Manager and
 * sub-components.
 * * This test suite validates the orchestration of the entire Warehouse System,
 * focusing on the interaction between multiple Manager instances and the
 * underlying Linux System V IPC mechanisms.
 * * Verified areas:
 * 1. IPC Lifecycle: Creation, attachment, and cleanup of SHM/SEM/MSG.
 * 2. Cross-Process Synchronization: Kernel-level blocking of threads/processes.
 * 3. Messaging: Reliable signaling via the Command Queue.
 * 4. Component Integration: End-to-end flows involving Belt, Dispatcher, Truck,
 * and SessionManager.
 */

#include "../include/Manager.h"
#include "../include/Shared.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

/**
 * @class ManagerTest
 * @brief Environment fixture for IPC integration testing.
 * * Perfroms "Hard Resets" of the Linux Kernel's IPC tables in both SetUp and
 * TearDown. This ensures that if a test fails or crashes, no stale segments,
 * semaphores, or message queues interfere with the next execution.
 */
class ManagerTest : public ::testing::Test {
protected:
  /**
   * @brief Prepares a clean environment for each integration test case.
   * * In System V IPC, resources (SHM, SEM, MSG) are persistent and live in the
   * kernel until explicitly removed. This method performs a "Hard Reset" by
   * marking existing resources for destruction (IPC_RMID).
   * * A 10ms synchronization delay is included to allow the Linux kernel to
   * finalize the release of descriptors, ensuring that the next Manager
   * instance starts with a completely fresh and zeroed state.
   */
  void SetUp() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0666), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0666), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0666), IPC_RMID, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  /**
   * @brief Performs post-test cleanup of system-wide IPC resources.
   * * This method acts as a safety net to prevent IPC resource leaks in the
   * Operating System. Even if a test fails or an assertion throws an exception,
   * TearDown ensures that the Shared Memory, Semaphores, and Message Queues
   * are freed, keeping the OS IPC tables clean for subsequent test runs or
   * manual execution.
   */
  void TearDown() override {
    shmctl(shmget(SHM_KEY_ID, 0, 0666), IPC_RMID, nullptr);
    semctl(semget(SEM_KEY_ID, 0, 0666), 0, IPC_RMID);
    msgctl(msgget(MSG_KEY_ID, 0666), IPC_RMID, nullptr);
  }
};

/**
 * @test InitializationAsOwner
 * @brief Verifies that the 'Owner' Manager correctly allocates and zeros out
 * the Shared Memory.
 */
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

/**
 * @test SharedMemorySync
 * @brief Verifies the "Shared" nature of the memory segment between an Owner
 * and a Client instance.
 */
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

/**
 * @test MessageQueueCommunication
 * @brief Validates non-blocking signal transmission and reception via System V
 * Message Queues.
 */
TEST_F(ManagerTest, MessageQueueCommunication) {
  Manager owner(true);
  Manager client(false);

  EXPECT_EQ(client.receiveSignalNonBlocking(), SIGNAL_NONE);
  owner.sendSignal(SIGNAL_DEPARTURE);

  SignalType received_signal = client.receiveSignalNonBlocking();
  EXPECT_EQ(received_signal, SIGNAL_DEPARTURE);
}

/**
 * @test SemaphoreSanity
 * @brief Ensures that basic Mutex operations (Lock/Unlock) do not throw or
 * deadlock in a single-threaded context.
 */
TEST_F(ManagerTest, SemaphoreSanity) {
  Manager manager(true);

  ASSERT_NO_THROW(manager.lockBelt());
  manager.getState()->current_items_count++;
  ASSERT_NO_THROW(manager.unlockBelt());

  ASSERT_NO_THROW(manager.lockDock());
  ASSERT_NO_THROW(manager.unlockDock());
}

/**
 * @test SemaphoreBlockingLogic
 * @brief Verifies physical thread blocking by the kernel when a Mutex is held
 * by another Manager.
 */
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

/**
 * @test SessionManager_BasicLifecycle
 * @brief Verifies login, process spawning limits, and logout flow for a single
 * session.
 */
TEST_F(ManagerTest, SessionManager_BasicLifecycle) {
  Manager mgr(true);
  ASSERT_TRUE(mgr.session_store->login("TestUser", UserRole::Operator, 100, 2));
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());
  EXPECT_FALSE(mgr.session_store->trySpawnProcess());

  mgr.session_store->reportProcessFinished();
  EXPECT_TRUE(mgr.session_store->trySpawnProcess());
}

/**
 * @test SessionManager_MultiUserIsolation
 * @brief Ensures that process spawn limits are tracked independently for each
 * user session.
 */
TEST_F(ManagerTest, SessionManager_MultiUserIsolation) {
  Manager admin(true);
  Manager guest(false);

  ASSERT_TRUE(admin.session_store->login("Admin", UserRole::SysAdmin, 0, 10));
  ASSERT_TRUE(guest.session_store->login("Guest", UserRole::Viewer, 0, 1));

  EXPECT_TRUE(admin.session_store->trySpawnProcess());
  EXPECT_TRUE(guest.session_store->trySpawnProcess());
  EXPECT_FALSE(guest.session_store->trySpawnProcess()); // Guest limit reached
  EXPECT_TRUE(admin.session_store->trySpawnProcess());  // Admin still has slots
}

/**
 * @test SessionManager_PreventDuplicateLogin
 * @brief Validates that the system prevents multiple sessions with the same
 * username.
 */
TEST_F(ManagerTest, SessionManager_PreventDuplicateLogin) {
  Manager mgr_1(true);
  Manager mgr_2(false);

  ASSERT_TRUE(
      mgr_1.session_store->login("Operator", UserRole::Operator, 100, 5));
  EXPECT_FALSE(
      mgr_2.session_store->login("Operator", UserRole::Operator, 100, 5));
}

/**
 * @test SessionManager_MaxSessionsLimit
 * @brief Verifies that the system respects the global MAX_USERS_SESSIONS limit.
 */
TEST_F(ManagerTest, SessionManager_MaxSessionsLimit) {
  Manager mgr(true);
  std::vector<std::unique_ptr<Manager>> clients;

  for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
    auto client_mgr = std::make_unique<Manager>(false);
    ASSERT_TRUE(client_mgr->session_store->login("User" + std::to_string(i),
                                                 UserRole::Viewer, i, 1));
    clients.push_back(std::move(client_mgr));
  }

  Manager overflow_client(false);
  EXPECT_FALSE(overflow_client.session_store->login("UserOverflow",
                                                    UserRole::Viewer, 99, 1));
}

/**
 * @test Belt_Integration_BasicPushPop
 * @brief Tests the basic circular buffer integration between Manager and Belt
 * components.
 */
TEST_F(ManagerTest, Belt_Integration_BasicPushPop) {
  Manager mgr(true);
  Package pkg_in;
  pkg_in.weight = 50.0;

  mgr.belt->push(pkg_in);
  EXPECT_EQ(mgr.getState()->current_items_count, 1);

  Package pkg_out = mgr.belt->pop();
  EXPECT_EQ(pkg_out.id, 1);
  EXPECT_EQ(mgr.getState()->current_items_count, 0);
}

/**
 * @test Belt_Integration_BlockingConsumer
 * @brief Verifies that a consumer blocks on an empty belt until a producer adds
 * a package.
 */
TEST_F(ManagerTest, Belt_Integration_BlockingConsumer) {
  Manager producer(true);
  std::atomic<bool> pop_finished{false};

  std::thread consumer_thread([&]() {
    Manager consumer(false);
    consumer.belt->pop(); // Blocks
    pop_finished = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(pop_finished);

  Package p;
  producer.belt->push(p);
  consumer_thread.join();
  EXPECT_TRUE(pop_finished);
}

/**
 * @test Belt_Integration_BlockingProducer
 * @brief Verifies that a producer blocks when the belt capacity is reached
 * until space is freed.
 */
TEST_F(ManagerTest, Belt_Integration_BlockingProducer) {
  Manager producer(true);
  for (int i = 0; i < MAX_BELT_CAPACITY_K; ++i) {
    Package p;
    producer.belt->push(p);
  }

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

/**
 * @test TruckComponentInitialization
 * @brief Verifies the Truck component is correctly instantiated by the Manager.
 */
TEST_F(ManagerTest, TruckComponentInitialization) {
  Manager mgr(true);
  EXPECT_NE(mgr.truck, nullptr);
}

/**
 * @test BlockingSignalReception
 * @brief Verifies that blocking signal reception correctly puts the process to
 * sleep until a signal arrives.
 */
TEST_F(ManagerTest, BlockingSignalReception) {
  Manager receiver(true);
  std::thread sender([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Manager sender_mgr(false);
    sender_mgr.sendSignal(SIGNAL_DEPARTURE);
  });

  SignalType received = receiver.receiveSignalBlocking();
  EXPECT_EQ(received, SIGNAL_DEPARTURE);
  sender.join();
}

/**
 * @test BlockingSignal_EndWork
 * @brief Verifies handling of the termination signal across different Manager
 * instances.
 */
TEST_F(ManagerTest, BlockingSignal_EndWork) {
  Manager receiver(true);
  std::thread sender([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Manager sender_mgr(false);
    sender_mgr.sendSignal(SIGNAL_END_WORK);
  });

  SignalType received = receiver.receiveSignalBlocking();
  EXPECT_EQ(received, SIGNAL_END_WORK);
  sender.join();
}

/**
 * @test Dispatcher_SuccessfulLoad
 * @brief End-to-end integration test for loading a package from the belt into a
 * present truck.
 */
TEST_F(ManagerTest, Dispatcher_SuccessfulLoad) {
  Manager m(true);
  m.lockDock();
  std::memset(&(m.getState()->dock_truck), 0, sizeof(TruckState));
  m.getState()->dock_truck.is_present = true;
  m.getState()->dock_truck.max_load = 5;
  m.unlockDock();

  Package p;
  p.weight = 20.0;
  m.belt->push(p);
  m.dispatcher->processNextPackage();

  m.lockDock();
  EXPECT_EQ(m.getState()->dock_truck.current_load, 1);
  EXPECT_DOUBLE_EQ(m.getState()->dock_truck.current_weight, 20.0);
  m.unlockDock();
}

/**
 * @test Dispatcher_FullTruckTriggersDeparture
 * @brief Verifies that the Dispatcher automatically sends a departure signal
 * when the truck's capacity is reached.
 */
TEST_F(ManagerTest, Dispatcher_FullTruckTriggersDeparture) {
  Manager m(true);
  m.lockDock();
  std::memset(&(m.getState()->dock_truck), 0, sizeof(TruckState));
  m.getState()->dock_truck.is_present = true;
  m.getState()->dock_truck.max_load = 1;
  m.unlockDock();

  Package p;
  m.belt->push(p);
  m.dispatcher->processNextPackage();

  SignalType sig = m.receiveSignalNonBlocking();
  EXPECT_EQ(sig, SIGNAL_DEPARTURE);
}
