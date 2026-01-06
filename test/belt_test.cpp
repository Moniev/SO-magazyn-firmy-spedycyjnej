/**
 * @file belt_test.cpp
 * @brief Unit tests for the Belt class logic using GoogleTest.
 * * These tests utilize a mocked (local) SharedState structure to verify the
 * mathematical and logical integrity of the conveyor belt's circular buffer
 * without requiring actual system-wide IPC resource allocation.
 */

#include "../include/Belt.h"
#include <cstring>
#include <gtest/gtest.h>

/**
 * @class BeltTest
 * @brief Test fixture for Belt logic verification.
 * * Initializes a clean memory state before each test to ensure isolation.
 */
class BeltTest : public ::testing::Test {
protected:
  /** @brief Local instance of the shared memory structure used as a mock. */
  SharedState mock_shared_memory;

  /** @brief Lambda used to satisfy functional requirements for non-blocking
   * tests. */
  std::function<void()> no_op = []() {};

  /**
   * @brief Resets the mock memory segment to zero before every test case.
   */
  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
  }
};

/**
 * @test PushUpdatesStateAndTail
 * @brief Verifies that adding a package correctly updates global metrics and
 * pointers.
 * * **Logic Check**:
 * - Increments `current_items_count`.
 * - Accumulates `current_belt_weight`.
 * - Moves the `tail` pointer to the next available index.
 * - Assigns a unique system-wide ID to the package.
 */
TEST_F(BeltTest, PushUpdatesStateAndTail) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);
  Package pkg_in;
  pkg_in.weight = 10.5;

  belt.push(pkg_in);

  EXPECT_EQ(mock_shared_memory.current_items_count, 1);
  EXPECT_DOUBLE_EQ(mock_shared_memory.current_belt_weight, 10.5);
  EXPECT_EQ(mock_shared_memory.total_packages_created, 1);
  EXPECT_EQ(mock_shared_memory.tail, 1);
  EXPECT_EQ(mock_shared_memory.head, 0);
  EXPECT_EQ(mock_shared_memory.belt[0].id, 1);
}

/**
 * @test PopReturnsCorrectDataAndUpdatesHead
 * @brief Ensures data retrieval integrity and memory cleanup.
 * * **Logic Check**:
 * - Confirms the package returned matches the one at the `head` index.
 * - Verifies the `head` pointer advances correctly.
 * - Checks that the memory slot is cleared (zeroed) after retrieval.
 */
TEST_F(BeltTest, PopReturnsCorrectDataAndUpdatesHead) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);
  Package manual_pkg;
  manual_pkg.id = 202;
  manual_pkg.weight = 5.0;

  mock_shared_memory.belt[0] = manual_pkg;
  mock_shared_memory.tail = 1;
  mock_shared_memory.current_items_count = 1;
  mock_shared_memory.current_belt_weight = 5.0;

  Package pkg_out = belt.pop();

  EXPECT_EQ(pkg_out.id, 202);
  EXPECT_DOUBLE_EQ(pkg_out.weight, 5.0);
  EXPECT_EQ(mock_shared_memory.current_items_count, 0);
  EXPECT_DOUBLE_EQ(mock_shared_memory.current_belt_weight, 0.0);
  EXPECT_EQ(mock_shared_memory.head, 1);
}

/**
 * @test CircularLogicWrapAround
 * @brief Validates the modulo arithmetic for the circular buffer.
 * * **Logic Check**:
 * - When the `tail` reaches the last index (`MAX_BELT_CAPACITY_K - 1`),
 * the next push must reset the `tail` to 0.
 */

TEST_F(BeltTest, CircularLogicWrapAround) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);
  int capacity = MAX_BELT_CAPACITY_K;
  mock_shared_memory.tail = capacity - 1;

  Package pkg;
  belt.push(pkg);

  EXPECT_EQ(mock_shared_memory.belt[capacity - 1].id, 1);
  EXPECT_EQ(mock_shared_memory.tail, 0);
}

/**
 * @test FIFOOrdering
 * @brief Confirms the First-In, First-Out (FIFO) nature of the belt.
 * * **Logic Check**:
 * - Multiple packages pushed in sequence must be popped in the exact same
 * order.
 */
TEST_F(BeltTest, FIFOOrdering) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);
  Package p1;
  p1.weight = 10;
  Package p2;
  p2.weight = 20;

  belt.push(p1);
  belt.push(p2);

  Package out1 = belt.pop();
  EXPECT_EQ(out1.id, 1);

  Package out2 = belt.pop();
  EXPECT_EQ(out2.id, 2);
}

/**
 * @test GetCountReturnsCorrectValue
 * @brief Simple getter verification.
 */
TEST_F(BeltTest, GetCountReturnsCorrectValue) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);
  mock_shared_memory.current_items_count = 5;
  EXPECT_EQ(belt.getCount(), 5);
}

/**
 * @test SafetyNullCheck
 * @brief Verifies the class's robustness against uninitialized Shared Memory
 * pointers.
 * * **Logic Check**:
 * - Prevents Segmentation Faults if the `shm` pointer is null.
 */
TEST_F(BeltTest, SafetyNullCheck) {
  Belt unsafe_belt(nullptr, no_op, no_op, no_op, no_op, no_op, no_op);
  Package p;
  ASSERT_NO_THROW(unsafe_belt.push(p));

  Package out = unsafe_belt.pop();
  EXPECT_EQ(out.id, 0);
}
