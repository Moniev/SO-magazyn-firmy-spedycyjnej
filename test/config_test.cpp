#include "../include/Config.h"
#include <gtest/gtest.h>

TEST(ConfigTest, ReturnsDefaultWhenEnvMissing) {
  unsetenv("NON_EXISTENT_VAR");

  std::string value = Config::get().getEnv("NON_EXISTENT_VAR", "default_value");
  EXPECT_EQ(value, "default_value");
}

TEST(ConfigTest, ReturnsEnvValueWhenSet) {
  setenv("TEST_MY_VAR", "12345", 1);

  std::string value = Config::get().getEnv("TEST_MY_VAR", "default");
  EXPECT_EQ(value, "12345");
}

TEST(ConfigTest, DispatchesLogLevelsCorrectly) {
  EXPECT_EQ(Config::dispatchLogLevel("debug"), spdlog::level::debug);
  EXPECT_EQ(Config::dispatchLogLevel("DEBUG"), spdlog::level::debug);
  EXPECT_EQ(Config::dispatchLogLevel("warn"), spdlog::level::warn);
  EXPECT_EQ(Config::dispatchLogLevel("err"), spdlog::level::err);
  EXPECT_EQ(Config::dispatchLogLevel("off"), spdlog::level::off);
}

TEST(ConfigTest, FallbackToInfoOnGarbage) {
  EXPECT_EQ(Config::dispatchLogLevel("random_string"), spdlog::level::info);
  EXPECT_EQ(Config::dispatchLogLevel(""), spdlog::level::info);
}

TEST(ConfigTest, LogicFlagsHandling) {
  setenv("LOG_TO_CONSOLE", "true", 1);
  EXPECT_EQ(Config::get().getEnv("LOG_TO_CONSOLE"), "true");

  setenv("LOG_TO_CONSOLE", "false", 1);
  EXPECT_EQ(Config::get().getEnv("LOG_TO_CONSOLE"), "false");
}
