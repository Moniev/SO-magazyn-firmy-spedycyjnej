/**
 * @file terminal_test.cpp
 * @brief Integration tests for the Command Line Interface (CLI).
 * * Uses stream hijacking (cin/cout redirection) to simulate user input.
 * * Mocks the IPC environment so the Test Process acts as both Truck and
 * Express service.
 */
#include "../include/Manager.h"
#include "../include/terminal/TerminalManager.h"
#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

std::atomic<bool> keep_running{true};

class TerminalTest : public ::testing::Test {
protected:
  Manager manager{true};

  std::stringstream mock_input;
  std::stringstream mock_output;
  std::streambuf *orig_cin;
  std::streambuf *orig_cout;

  void SetUp() override {
    keep_running.store(true);

    orig_cin = std::cin.rdbuf();
    orig_cout = std::cout.rdbuf();
    std::cin.rdbuf(mock_input.rdbuf());
    std::cout.rdbuf(mock_output.rdbuf());

    while (manager.receiveSignalNonBlocking(getpid()) != SIGNAL_NONE) {
    }

    manager.session_store->login("Admin", UserRole::SysAdmin, 0, 1);
    manager.lockDock();
    manager.getState()->dock_truck.is_present = true;
    manager.getState()->dock_truck.id = getpid();
    manager.unlockDock();

    manager.session_store->login("System-Express", UserRole::Operator, 0, 2);
  }

  void TearDown() override {
    manager.session_store->logout();

    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);
  }

  void runTestLoop(TerminalManager &tm) {
    int safety_breaker = 0;
    while (manager.getState()->running && safety_breaker < 20) {
      if (mock_input.peek() == EOF)
        break;

      tm.runOnce();

      safety_breaker++;
    }
  }
};

TEST_F(TerminalTest, SendsVipSignal) {
  mock_input << "vip\nexit\n";
  TerminalManager terminal(&manager);

  runTestLoop(terminal);

  SignalType sig = manager.receiveSignalNonBlocking(getpid());
  EXPECT_EQ(sig, SIGNAL_EXPRESS_LOAD);
}

TEST_F(TerminalTest, SendsDepartSignal) {
  mock_input << "depart\nexit\n";
  TerminalManager terminal(&manager);

  runTestLoop(terminal);

  SignalType sig = manager.receiveSignalNonBlocking(getpid());
  EXPECT_EQ(sig, SIGNAL_DEPARTURE);
}

TEST_F(TerminalTest, SendsStopSignal) {
  manager.session_store->logout();
  manager.session_store->login("Root", UserRole::SysAdmin, 0, 1);

  mock_input << "stop\n";
  TerminalManager terminal(&manager);

  runTestLoop(terminal);

  EXPECT_FALSE(manager.getState()->running);
  SignalType sig = manager.receiveSignalNonBlocking(getpid());
  EXPECT_EQ(sig, SIGNAL_END_WORK);
}

TEST_F(TerminalTest, PermissionDeniedForViewer) {
  manager.session_store->logout();
  manager.session_store->login("ViewerUser", UserRole::Viewer, 0, 1);

  mock_input << "vip\nexit\n";

  TerminalManager terminal(&manager);
  runTestLoop(terminal);
  EXPECT_EQ(manager.receiveSignalNonBlocking(getpid()), SIGNAL_NONE);
}
