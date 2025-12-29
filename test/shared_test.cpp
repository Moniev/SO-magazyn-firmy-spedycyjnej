#include "../include/Shared.h"
#include <gtest/gtest.h>

TEST(SharedSpecsTest, BitwiseFlagsLogic) {
  PackageType mask = PackageType::TypeA | PackageType::TypeC;

  EXPECT_TRUE(hasFlag(mask, PackageType::TypeA));
  EXPECT_TRUE(hasFlag(mask, PackageType::TypeC));
  EXPECT_FALSE(hasFlag(mask, PackageType::TypeB));

  PackageType intersection = mask & PackageType::TypeA;
  EXPECT_EQ(intersection, PackageType::TypeA);
}

TEST(SharedSpecsTest, ActionTypeComposition) {
  ActionType action = ActionType::Created | ActionType::ByWorker;

  EXPECT_TRUE(hasFlag(action, ActionType::Created));
  EXPECT_TRUE(hasFlag(action, ActionType::ByWorker));

  EXPECT_FALSE(hasFlag(action, ActionType::ByTruck));
}

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
