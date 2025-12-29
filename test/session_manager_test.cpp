#include "../include/SessionManager.h"
#include <gtest/gtest.h>
#include <vector>

class SessionManagerTest : public ::testing::Test {
protected:
  SharedState mock_shared_memory;

  std::function<void()> no_op_lock = []() {};
  std::function<void()> no_op_unlock = []() {};

  void SetUp() override {
    std::memset(&mock_shared_memory, 0, sizeof(SharedState));
  }
};

TEST_F(SessionManagerTest, DefaultStateIsSafe) {
  SessionManager sm(&mock_shared_memory, no_op_lock, no_op_unlock);

  EXPECT_EQ(sm.getSessionIndex(), -1);
  EXPECT_FALSE(sm.trySpawnProcess());
}

TEST_F(SessionManagerTest, LoginSpawnLogoutFlow) {
  SessionManager sm(&mock_shared_memory, no_op_lock, no_op_unlock);

  ASSERT_TRUE(sm.login("Tester", 2));
  EXPECT_NE(sm.getSessionIndex(), -1);

  int idx = sm.getSessionIndex();

  EXPECT_TRUE(mock_shared_memory.users[idx].active);
  EXPECT_STREQ(mock_shared_memory.users[idx].username, "Tester");
  EXPECT_EQ(mock_shared_memory.users[idx].max_processes, 2);

  EXPECT_TRUE(sm.trySpawnProcess());
  EXPECT_EQ(mock_shared_memory.users[idx].current_processes, 1);

  sm.logout();

  EXPECT_FALSE(mock_shared_memory.users[idx].active);
  EXPECT_EQ(sm.getSessionIndex(), -1);
}

TEST_F(SessionManagerTest, ProcessLimitsEnforcement) {
  SessionManager sm(&mock_shared_memory, no_op_lock, no_op_unlock);
  sm.login("LimitUser", 2);

  EXPECT_TRUE(sm.trySpawnProcess());
  EXPECT_TRUE(sm.trySpawnProcess());

  EXPECT_FALSE(sm.trySpawnProcess());
  sm.reportProcessFinished();

  EXPECT_TRUE(sm.trySpawnProcess());
}

TEST_F(SessionManagerTest, PreventDuplicateUsernames) {
  SessionManager sm_1(&mock_shared_memory, no_op_lock, no_op_unlock);
  SessionManager sm_2(&mock_shared_memory, no_op_lock, no_op_unlock);

  ASSERT_TRUE(sm_1.login("UniqueUser", 5));
  EXPECT_FALSE(sm_2.login("UniqueUser", 5));
  EXPECT_TRUE(sm_2.login("OtherUser", 5));
}

TEST_F(SessionManagerTest, MaxUsersSaturation) {
  std::vector<std::unique_ptr<SessionManager>> managers;

  for (int i = 0; i < MAX_USERS_SESSIONS; ++i) {
    auto sm = std::make_unique<SessionManager>(&mock_shared_memory, no_op_lock,
                                               no_op_unlock);
    std::string name = "User" + std::to_string(i);
    ASSERT_TRUE(sm->login(name, 1)) << "Failed to login user " << i;
    managers.push_back(std::move(sm));
  }

  SessionManager overflow_sm(&mock_shared_memory, no_op_lock, no_op_unlock);
  EXPECT_FALSE(overflow_sm.login("OverflowUser", 1));
}

TEST_F(SessionManagerTest, NullSharedMemorySafety) {
  SessionManager unsafe_sm(nullptr, no_op_lock, no_op_unlock);

  EXPECT_FALSE(unsafe_sm.login("Ghost", 1));
  EXPECT_FALSE(unsafe_sm.trySpawnProcess());

  ASSERT_NO_THROW(unsafe_sm.logout());
  ASSERT_NO_THROW(unsafe_sm.reportProcessFinished());
}

TEST_F(SessionManagerTest, ProcessCountUnderflow) {
  SessionManager sm(&mock_shared_memory, no_op_lock, no_op_unlock);
  ASSERT_TRUE(sm.login("MathUser", 5));

  sm.reportProcessFinished();

  int idx = sm.getSessionIndex();
  EXPECT_EQ(mock_shared_memory.users[idx].current_processes, 0);

  EXPECT_TRUE(sm.trySpawnProcess());
  sm.reportProcessFinished();
  EXPECT_EQ(mock_shared_memory.users[idx].current_processes, 0);
}
