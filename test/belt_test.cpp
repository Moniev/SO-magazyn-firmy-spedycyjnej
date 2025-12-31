#include "../include/Belt.h"
#include <cstring>
#include <gtest/gtest.h>

class BeltTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;

  std::function<void()> no_op = []() {};

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
  }
};

TEST_F(BeltTest, PushUpdatesStateAndTail) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);

  Package pkg_in;
  pkg_in.id = 101;
  pkg_in.weight = 10.5;

  belt.push(pkg_in);

  EXPECT_EQ(mock_shared_memory.current_items_count, 1);
  EXPECT_DOUBLE_EQ(mock_shared_memory.current_belt_weight, 10.5);
  EXPECT_EQ(mock_shared_memory.total_packages_created, 1);

  EXPECT_EQ(mock_shared_memory.tail, 1);
  EXPECT_EQ(mock_shared_memory.head, 0);

  EXPECT_EQ(mock_shared_memory.belt[0].id, 101);
}

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
  EXPECT_EQ(mock_shared_memory.tail, 1);
}

TEST_F(BeltTest, CircularLogicWrapAround) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);

  int capacity = MAX_BELT_CAPACITY_K;
  mock_shared_memory.tail = capacity - 1;

  Package pkg;
  pkg.id = 999;

  belt.push(pkg);

  EXPECT_EQ(mock_shared_memory.belt[capacity - 1].id, 999);
  EXPECT_EQ(mock_shared_memory.tail, 0);
}

TEST_F(BeltTest, FIFOOrdering) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);

  Package p1;
  p1.id = 1;
  p1.weight = 10;
  Package p2;
  p2.id = 2;
  p2.weight = 20;

  belt.push(p1);
  belt.push(p2);

  EXPECT_EQ(mock_shared_memory.current_items_count, 2);
  EXPECT_DOUBLE_EQ(mock_shared_memory.current_belt_weight, 30.0);

  Package out1 = belt.pop();
  EXPECT_EQ(out1.id, 1);

  Package out2 = belt.pop();
  EXPECT_EQ(out2.id, 2);

  EXPECT_EQ(mock_shared_memory.current_items_count, 0);
}

TEST_F(BeltTest, GetCountReturnsCorrectValue) {
  Belt belt(&mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op);

  EXPECT_EQ(belt.getCount(), 0);

  mock_shared_memory.current_items_count = 5;
  EXPECT_EQ(belt.getCount(), 5);
}

TEST_F(BeltTest, SafetyNullCheck) {
  Belt unsafe_belt(nullptr, no_op, no_op, no_op, no_op, no_op, no_op);

  Package p;
  ASSERT_NO_THROW(unsafe_belt.push(p));

  Package out = unsafe_belt.pop();
  EXPECT_EQ(out.id, 0);
  EXPECT_EQ(unsafe_belt.getCount(), 0);
}
