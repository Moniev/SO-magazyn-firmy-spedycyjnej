/**
 * @file terminal_test.cpp
 * @brief Integration tests for the Command Line Interface (CLI).
 * * These tests verify the frontend logic of the application by simulating user
 * keystrokes and observing the resulting side effects in the IPC system.
 * * Key Testing Techniques:
 * 1. **Stream Hijacking**: Redirects `std::cin` and `std::cout` to in-memory
 * `std::stringstream` buffers to automate console interaction.
 * 2. **RBAC Verification**: Ensures that restricted commands (like 'stop')
 * are rejected when executed by unauthorized users.
 */

#include "../include/terminal/TerminalManager.h"
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

/**
 * @class TerminalTest
 * @brief Test fixture that intercepts Standard Input/Output streams.
 * * This fixture allows us to test an interactive console application as if
 * it were a non-interactive function.
 */
class TerminalTest : public ::testing::Test {
protected:
  /** @brief Real instance of Manager (Owner) to handle actual IPC signals. */
  Manager manager{true};

  /** @name Stream Mocking Buffers
   * @{ */
  std::stringstream mock_input;  /**< Acts as the fake keyboard input. */
  std::stringstream mock_output; /**< Captures console output (cout). */
  std::streambuf *orig_cin;      /**< Backup of the original stdin buffer. */
  std::streambuf *orig_cout;     /**< Backup of the original stdout buffer. */
  /** @} */

  /**
   * @brief Sets up the test environment.
   * 1. Swaps standard streams with local stringstreams.
   * 2. Flushes any stale signals from the message queue.
   * 3. Logs in a 'SysAdmin' user by default to allow full command access.
   */
  void SetUp() override {
    orig_cin = std::cin.rdbuf();
    orig_cout = std::cout.rdbuf();

    std::cin.rdbuf(mock_input.rdbuf());
    std::cout.rdbuf(mock_output.rdbuf());

    while (manager.receiveSignalNonBlocking() != SIGNAL_NONE) {
    }

    manager.session_store->login("TestTerminalUser", UserRole::SysAdmin, 0, 1);
  }

  /**
   * @brief Restores the environment.
   * 1. Logs out the test user.
   * 2. Restores `std::cin` and `std::cout` to preventing breaking GTest output.
   */
  void TearDown() override {
    manager.session_store->logout();

    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);
  }
};

/**
 * @test SendsVipSignal
 * @brief Verifies that the 'vip' command triggers an EXPRESS_LOAD signal.
 * * Scenario: User types "vip" -> Enter -> "exit" -> Enter.
 */
TEST_F(TerminalTest, SendsVipSignal) {
  mock_input << "vip\nexit\n";

  TerminalManager terminal(&manager);
  terminal.run();

  SignalType received = manager.receiveSignalNonBlocking();
  EXPECT_EQ(received, SIGNAL_EXPRESS_LOAD);
}

/**
 * @test SendsDepartSignal
 * @brief Verifies that the 'depart' command triggers a DEPARTURE signal.
 */
TEST_F(TerminalTest, SendsDepartSignal) {
  mock_input << "depart\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_DEPARTURE);
}

/**
 * @test SendsStopSignal
 * @brief Verifies that the 'stop' command triggers a system-wide END_WORK
 * signal.
 * * Note: The 'stop' command also internally sets the loop flag to false,
 * so explicit 'exit' input is not strictly required, but good practice.
 */
TEST_F(TerminalTest, SendsStopSignal) {
  mock_input << "stop\n";

  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_END_WORK);
}

/**
 * @test HandlesCaseInsensitivity
 * @brief Ensures usability by verifying case-insensitive command parsing.
 * * Input: "ViP" should be treated exactly like "vip".
 */
TEST_F(TerminalTest, HandlesCaseInsensitivity) {
  mock_input << "ViP\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_EXPRESS_LOAD);
}

/**
 * @test HandlesUnknownCommandGracefully
 * @brief Robustness test for invalid inputs.
 * * Ensures the application does not crash or send random signals when
 * garbage text is entered.
 */
TEST_F(TerminalTest, HandlesUnknownCommandGracefully) {
  mock_input << "abra_kadabra\nexit\n";

  TerminalManager terminal(&manager);

  ASSERT_NO_THROW(terminal.run());
  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}

/**
 * @test HelpCommandDoesNotSendSignal
 * @brief Verifies that local-only commands (like 'help') do not generate
 * network noise.
 */
TEST_F(TerminalTest, HelpCommandDoesNotSendSignal) {
  mock_input << "help\nexit\n";
  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}

/**
 * @test PermissionDeniedForViewer
 * @brief Verifies Role-Based Access Control (RBAC) enforcement in the CLI.
 * *
 * * Scenario:
 * 1. Logout the default Admin.
 * 2. Login as a 'Viewer'.
 * 3. Attempt to execute 'vip' (which requires Operator/Admin role).
 * * Expected: The command is rejected locally, and NO signal is sent to IPC.
 */
TEST_F(TerminalTest, PermissionDeniedForViewer) {
  manager.session_store->logout();
  manager.session_store->login("ViewerUser", UserRole::Viewer, 0, 1);
  mock_input << "vip\nexit\n";

  TerminalManager terminal(&manager);
  terminal.run();

  EXPECT_EQ(manager.receiveSignalNonBlocking(), SIGNAL_NONE);
}
