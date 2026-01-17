/**
 * @file ConfigTest.cpp
 * @brief Unit tests for the Global Configuration Manager.
 * * These tests verify the correct interaction between the application and
 * the system environment variables (OS level), as well as the logic for
 * mapping string configurations to internal spdlog enums.
 */

#include "../include/Config.h"
#include <cstdlib>
#include <gtest/gtest.h>

/**
 * @test ReturnsDefaultWhenEnvMissing
 * @brief Verifies the fallback mechanism for missing environment variables.
 * * Scenario:
 * 1. Explicitly unset a specific environment variable to ensure it doesn't
 * exist.
 * 2. Call getEnv() requesting this variable with a fallback default.
 * * Expected Result:
 * - The method returns the 'default_value' provided in the second argument.
 */
TEST(ConfigTest, ReturnsDefaultWhenEnvMissing) {
  unsetenv("NON_EXISTENT_VAR");

  std::string value = Config::get().getEnv("NON_EXISTENT_VAR", "default_value");
  EXPECT_EQ(value, "default_value");
}

/**
 * @test ReturnsEnvValueWhenSet
 * @brief Verifies that system environment variables take precedence.
 * * Scenario:
 * 1. Set a system environment variable using setenv().
 * 2. Call getEnv() requesting this variable.
 * * Expected Result:
 * - The method returns the actual value from the system ("12345"), ignoring the
 * default.
 */
TEST(ConfigTest, ReturnsEnvValueWhenSet) {
  setenv("TEST_MY_VAR", "12345", 1);

  std::string value = Config::get().getEnv("TEST_MY_VAR", "default");
  EXPECT_EQ(value, "12345");
}

/**
 * @test DispatchesLogLevelsCorrectly
 * @brief Verifies the string-to-enum mapping for logging levels.
 * * Scenario:
 * 1. Pass various valid log level strings to dispatchLogLevel().
 * 2. Include mixed-case strings (e.g., "DEBUG") to test normalization.
 * * Expected Result:
 * - Correct spdlog::level::level_enum values are returned.
 * - Case sensitivity is handled (e.g., "DEBUG" == "debug").
 */
TEST(ConfigTest, DispatchesLogLevelsCorrectly) {
  EXPECT_EQ(Config::dispatchLogLevel("debug"), spdlog::level::debug);
  EXPECT_EQ(Config::dispatchLogLevel("DEBUG"), spdlog::level::debug);
  EXPECT_EQ(Config::dispatchLogLevel("warn"), spdlog::level::warn);
  EXPECT_EQ(Config::dispatchLogLevel("err"), spdlog::level::err);
  EXPECT_EQ(Config::dispatchLogLevel("off"), spdlog::level::off);
}

/**
 * @test FallbackToInfoOnGarbage
 * @brief Verifies robustness against invalid configuration inputs.
 * * Scenario:
 * 1. Pass invalid strings (garbage data) or empty strings to
 * dispatchLogLevel().
 * * Expected Result:
 * - The system safely defaults to spdlog::level::info.
 * - No exceptions are thrown.
 */
TEST(ConfigTest, FallbackToInfoOnGarbage) {
  EXPECT_EQ(Config::dispatchLogLevel("random_string"), spdlog::level::info);
  EXPECT_EQ(Config::dispatchLogLevel(""), spdlog::level::info);
}

/**
 * @test LogicFlagsHandling
 * @brief Verifies retrieval of boolean-like configuration flags.
 * * Scenario:
 * 1. Set environment variables to "true" and "false".
 * 2. Retrieve them via getEnv().
 * * Expected Result:
 * - The method returns the exact string representation, allowing higher-level
 * logic to parse them as booleans.
 */
TEST(ConfigTest, LogicFlagsHandling) {
  setenv("LOG_TO_CONSOLE", "true", 1);
  EXPECT_EQ(Config::get().getEnv("LOG_TO_CONSOLE"), "true");

  setenv("LOG_TO_CONSOLE", "false", 1);
  EXPECT_EQ(Config::get().getEnv("LOG_TO_CONSOLE"), "false");
}
