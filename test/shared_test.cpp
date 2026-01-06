/**
 * @file shared_specs_test.cpp
 * @brief Unit tests for Shared Memory data structures and helper functions.
 * * Unlike the IPC integration tests, these unit tests verify the internal
 * logic of the `Package` structure and the bitwise arithmetic used for flags.
 * * Key verification areas:
 * 1. **Bitmask Operators**: Ensuring custom `|` and `&` operators work for
 * `enum class`.
 * 2. **Audit Trail Logic**: Verifying that `pushAction` correctly updates
 * package history.
 * 3. **Memory Safety**: Ensuring the fixed-size history buffer does not
 * overflow.
 */

#include "../include/Shared.h"
#include <gtest/gtest.h>

/**
 * @test BitwiseFlagsLogic
 * @brief Verifies the custom bitwise operators for `PackageType`.
 * * Since `enum class` does not support bitwise operations by default,
 * this test ensures the overloaded operators correctly set and retrieve flags.
 * *
 */
TEST(SharedSpecsTest, BitwiseFlagsLogic) {
  PackageType mask = PackageType::TypeA | PackageType::TypeC;

  EXPECT_TRUE(hasFlag(mask, PackageType::TypeA));
  EXPECT_TRUE(hasFlag(mask, PackageType::TypeC));
  EXPECT_FALSE(hasFlag(mask, PackageType::TypeB));

  PackageType intersection = mask & PackageType::TypeA;
  EXPECT_EQ(intersection, PackageType::TypeA);
}

/**
 * @test ActionTypeComposition
 * @brief Verifies the composition of complex event descriptions.
 * * Ensures that an action can define both "What happened" (e.g., Created)
 * and "Who did it" (e.g., ByWorker) within a single byte.
 */
TEST(SharedSpecsTest, ActionTypeComposition) {
  ActionType action = ActionType::Created | ActionType::ByWorker;

  EXPECT_TRUE(hasFlag(action, ActionType::Created));
  EXPECT_TRUE(hasFlag(action, ActionType::ByWorker));

  EXPECT_FALSE(hasFlag(action, ActionType::ByTruck));
}

/**
 * @test PushActionUpdatesState
 * @brief Verifies the integrity of the audit trail recording.
 * * Scenario: A worker performs an action on a package.
 * * Checks:
 * - History count increments.
 * - `editor_pid` is updated to the worker's PID.
 * - Timestamp is generated (non-zero).
 * - The action record is correctly stored in the array.
 */
TEST(SharedSpecsTest, PushActionUpdatesState) {
  Package p;
  p.history_count = 0;
  p.creator_pid = 100;

  pid_t workerPid = 123;
  p.pushAction(ActionType::Created, workerPid);

  EXPECT_EQ(p.history_count, 1);
  EXPECT_EQ(p.editor_pid, workerPid);
  EXPECT_GT(p.updated_at, 0);

  EXPECT_EQ(p.history[0].actor_pid, workerPid);
  EXPECT_EQ(p.history[0].type, ActionType::Created);
}

/**
 * @test PushActionBoundaryCheck
 * @brief Verifies Buffer Overflow protection in Shared Memory.
 * * **Critical Safety Check**: The `Package` struct is stored in a fixed-size
 * shared memory segment. Writing past `MAX_PACKAGE_HISTORY` would corrupt
 * adjacent memory (e.g., the next package on the belt).
 * * This test attempts to push more actions than the limit and verifies
 * that the count clamps at `MAX_PACKAGE_HISTORY`.
 */
TEST(SharedSpecsTest, PushActionBoundaryCheck) {
  Package p;
  p.history_count = 0;

  int limit = MAX_PACKAGE_HISTORY + 2;

  for (int i = 0; i < limit; ++i) {
    p.pushAction(ActionType::PlacedOnBelt, i);
  }

  EXPECT_EQ(p.history_count, MAX_PACKAGE_HISTORY);

  EXPECT_EQ(p.history[MAX_PACKAGE_HISTORY - 1].actor_pid,
            MAX_PACKAGE_HISTORY - 1);
}
