/**
 * @file Config.h
 * @brief Streamlined configuration with a robust log dispatcher.
 */

#pragma once
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <string>

/**
 * @class Config
 * @brief Singleton class responsible for runtime configuration.
 * * Acts as a bridge between the operating system's environment variables
 * and the application's internal settings. It specifically manages the
 * initialization of the logging system (spdlog), allowing for dynamic
 * control over log output destinations and verbosity without recompilation.
 */
class Config {
public:
  /**
   * @brief Retrieves the singleton instance of the Config class.
   * * Uses the "Meyers' Singleton" pattern to ensure thread-safe initialization
   * on the first call.
   * @return Reference to the single Config instance.
   */
  static Config &get() {
    static Config instance;
    return instance;
  }

  /**
   * @brief Fetches the value of an environment variable.
   * * Wraps the standard std::getenv safely.
   * @param key The name of the environment variable (e.g., "LOG_LEVEL").
   * @param defaultVal The value to return if the variable is not set in the OS.
   * @return The environment variable's value or the defaultVal.
   */
  std::string getEnv(const std::string &key,
                     const std::string &defaultVal = "") {
    char *raw = std::getenv(key.c_str());
    return raw ? std::string(raw) : defaultVal;
  }

  /**
   * @brief Maps a string representation to an spdlog level enum.
   * * Handles case-insensitivity by normalizing the input string to lowercase.
   * * Supported levels: trace, debug, info, warn, err, crit, off.
   * @param level The string representation of the log level (e.g., "DEBUG",
   * "debug").
   * @return The corresponding spdlog::level::level_enum. Defaults to 'info'
   * if the string is not recognized.
   */
  static spdlog::level::level_enum dispatchLogLevel(const std::string &level) {
    static const std::map<std::string, spdlog::level::level_enum> levels = {
        {"trace", spdlog::level::trace}, {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},   {"warn", spdlog::level::warn},
        {"err", spdlog::level::err},     {"crit", spdlog::level::critical},
        {"off", spdlog::level::off}};

    std::string level_lower = level;
    std::transform(level_lower.begin(), level_lower.end(), level_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    auto it = levels.find(level_lower);
    return (it != levels.end()) ? it->second : spdlog::level::info;
  }

  /**
   * @brief Configures the global spdlog logger based on environment settings.
   * * Reads the following environment variables:
   * - LOG_TO_CONSOLE: "true"/"false" (default: true)
   * - LOG_TO_FILE: "true"/"false" (default: false)
   * - LOG_LEVEL: "trace"..."off" (default: info)
   *
   * * Creates a combined logger that can write to one file simultaneusly.
   *
   * * Sets the global default logger for the application.
   *
   * @param proc_name The name of the process (used for the log filename, e.g.,
   * "logs/belt.log").
   */
  void setupLogger(const std::string &proc_name) {
    bool to_console = getEnv("LOG_TO_CONSOLE", "true") == "true";
    bool to_file = getEnv("LOG_TO_FILE", "true") == "true";
    std::string level_str = getEnv("LOG_LEVEL", "info");

    try {
      std::vector<spdlog::sink_ptr> sinks;

      if (to_console) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
      }

      if (to_file) {
        std::string log_path = "logs/simulation_report.txt";
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            log_path, false));
      }

      auto logger = std::make_shared<spdlog::logger>(proc_name, sinks.begin(),
                                                     sinks.end());

      logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");
      logger->set_level(dispatchLogLevel(level_str));
      spdlog::set_default_logger(logger);
      spdlog::flush_on(spdlog::level::info);

    } catch (const spdlog::spdlog_ex &ex) {
      fprintf(stderr, "Log initialization failed: %s\n", ex.what());
    }
  }
};
