#include "../include/terminal/TerminalManager.h"
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

class TerminalTest : public ::testing::Test {
protected:
  Manager manager{true};

  std::stringstream mock_input;
  std::stringstream mock_output;
  std::streambuf *orig_cin;
  std::streambuf *orig_cout;

  void SetUp() override {
    orig_cin = std::cin.rdbuf();
    orig_cout = std::cout.rdbuf();

    std::cin.rdbuf(mock_input.rdbuf());
    std::cout.rdbuf(mock_output.rdbuf());

    while (manager.receiveSignalNonBlocking() != SIGNAL_NONE) {
    }

    manager.session_store->login("TestTerminalUser", UserRole::SysAdmin, 0, 1);
  }

  void TearDown() override {
    manager.session_store->logout();

    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);
  }
};

TEST_F(TerminalTest, SendsVipSignal) {
  mock_input << "vip\nexit\n";

  TerminalManager terminal(&manager);
  terminal.run();

  SignalType received = manager.receiveSignalNonBlocking();
  EXPECT_EQ(received, SIGNAL_EXPRESS_LOAD);
}

TEST_F(TerminalTest, SendsDepartSignal) {
  mock_input << "depart\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_DEPARTURE);
}

TEST_F(TerminalTest, SendsStopSignal) {
  mock_input << "stop\n";

  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_END_WORK);
}

TEST_F(TerminalTest, HandlesCaseInsensitivity) {
  mock_input << "ViP\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_EXPRESS_LOAD);
}

TEST_F(TerminalTest, HandlesUnknownCommandGracefully) {
  mock_input << "abra_kadabra\nexit\n";

  TerminalManager terminal(&manager);

  ASSERT_NO_THROW(terminal.run());
  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}

TEST_F(TerminalTest, HelpCommandDoesNotSendSignal) {
  mock_input << "help\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}

TEST_F(TerminalTest, PermissionDeniedForViewer) {
  manager.session_store->logout();

  manager.session_store->login("ViewerUser", UserRole::Viewer, 0, 1);
  mock_input << "vip\nexit\n";

  TerminalManager terminal(&manager);
  terminal.run();
  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}
