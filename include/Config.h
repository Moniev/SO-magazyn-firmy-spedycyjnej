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

class Config {
public:
  static Config &get() {
    static Config instance;
    return instance;
  }

  std::string getEnv(const std::string &key,
                     const std::string &defaultVal = "") {
    char *raw = std::getenv(key.c_str());
    return raw ? std::string(raw) : defaultVal;
  }

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

  void setupLogger(const std::string &proc_name) {
    bool to_console = getEnv("LOG_TO_CONSOLE", "true") == "true";
    bool to_file = getEnv("LOG_TO_FILE", "false") == "true";
    std::string level_str = getEnv("LOG_LEVEL", "info");

    try {
      std::vector<spdlog::sink_ptr> sinks;

      if (to_console) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
      }

      if (to_file) {
        std::string log_path = "logs/" + proc_name + ".log";
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            log_path, true));
      }

      auto logger = std::make_shared<spdlog::logger>(proc_name, sinks.begin(),
                                                     sinks.end());

      logger->set_level(dispatchLogLevel(level_str));

      spdlog::set_default_logger(logger);
      spdlog::flush_on(spdlog::level::info);

    } catch (const spdlog::spdlog_ex &ex) {
      fprintf(stderr, "Log initialization failed: %s\n", ex.what());
    }
  }
};
