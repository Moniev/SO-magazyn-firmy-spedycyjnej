/**
 * @file worker_test.cpp
 * @brief Unit and Integration tests for the Worker class.
 */

#include "../include/Belt.h"
#include "../include/Manager.h"
#include "../include/Worker.h"
#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

class TestableManager : public Manager {
public:
  TestableManager() : Manager(true) {}

  void injectMockShm(SharedState *mock) { this->shm = mock; }

  ~TestableManager() { this->shm = nullptr; }
};

class WorkerTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;
  std::function<void()> no_op = []() {};

  TestableManager *test_manager;

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));

    test_manager = new TestableManager();
    test_manager->injectMockShm(&mock_shared_memory);
    test_manager->belt.reset(new Belt(&mock_shared_memory, no_op, no_op, no_op,
                                      no_op, no_op, no_op));

    test_manager->session_store.reset(
        new SessionManager(&mock_shared_memory, no_op, no_op));

    test_manager->getState()->running = true;
  }

  void TearDown() override { delete test_manager; }
};

TEST_F(WorkerTest, WorkerRegistrationLifecycle) {
  Worker worker(test_manager, 101);
  std::thread t([&worker]() { worker.run(); });

  int retries = 20;
  while (mock_shared_memory.current_workers_count == 0 && retries > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    retries--;
  }

  EXPECT_EQ(mock_shared_memory.current_workers_count, 1)
      << "Worker failed to register on startup";

  worker.stop();
  if (t.joinable())
    t.join();

  EXPECT_EQ(mock_shared_memory.current_workers_count, 0)
      << "Worker failed to unregister on exit";
}

TEST_F(WorkerTest, WorkerGeneratesPackages) {
  test_manager->session_store->login("test_worker", UserRole::Operator, 0, 10);

  Worker worker(test_manager, 102);
  std::thread t([&worker]() { worker.run(); });

  int max_wait_ms = 1500;
  while (mock_shared_memory.total_packages_created == 0 && max_wait_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    max_wait_ms -= 50;
  }

  worker.stop();
  if (t.joinable())
    t.join();

  EXPECT_GT(mock_shared_memory.total_packages_created, 0)
      << "Worker did not produce any packages (trySpawnProcess failed?)";
  EXPECT_GT(mock_shared_memory.current_items_count, 0)
      << "Belt is empty despite worker running";
  int tail_idx = (mock_shared_memory.tail > 0) ? mock_shared_memory.tail - 1
                                               : MAX_BELT_CAPACITY_K - 1;
  Package last_pkg = mock_shared_memory.belt[tail_idx];
  EXPECT_GE(last_pkg.weight, 1.0);
}

TEST_F(WorkerTest, WorkerRespectsFullBelt) {
  mock_shared_memory.current_items_count = MAX_BELT_CAPACITY_K;
  test_manager->session_store->login("test_worker", UserRole::Operator, 0, 10);

  Worker worker(test_manager, 103);
  std::thread t([&worker]() { worker.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  worker.stop();
  if (t.joinable())
    t.join();

  EXPECT_EQ(mock_shared_memory.current_items_count, MAX_BELT_CAPACITY_K);
  EXPECT_EQ(mock_shared_memory.current_workers_count, 0);
}

TEST_F(WorkerTest, WorkerStopsOnManagerSignal) {
  Worker worker(test_manager, 104);
  std::thread t([&worker]() { worker.run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(mock_shared_memory.current_workers_count, 1);

  test_manager->getState()->running = false;

  if (t.joinable())
    t.join();

  EXPECT_EQ(mock_shared_memory.current_workers_count, 0);
}
