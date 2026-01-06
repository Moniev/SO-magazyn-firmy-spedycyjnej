/**
 * @file CommandResolver.h
 * @brief Utility for mapping raw string inputs to typed command enumerations.
 */

#pragma once

#include <string>
#include <unordered_map>

/**
 * @enum CliCommand
 * @brief Strongly typed identifiers for available terminal commands.
 */
enum class CliCommand {
  Unknown,
  Vip,    /**< Trigger a high-priority VIP package. */
  Depart, /**< Force the current truck to depart. */
  Stop,   /**< Emergency system shutdown. */
  Help,   /**< Display the menu. */
  Exit    /**< Terminate the CLI session (not the system). */
};

/**
 * @class CommandResolver
 * @brief Static helper class for command string parsing.
 * * Uses an internal hash map to provide O(1) lookup for command strings.
 */
class CommandResolver {
public:
  /**
   * @brief Translates a string input into a CliCommand enum.
   * @param cmd The raw input string (case-sensitive, usually pre-processed to
   * lowercase).
   * @return CliCommand The corresponding enum value, or CliCommand::Unknown if
   * not found.
   */
  static CliCommand resolve(const std::string &cmd) {
    static const std::unordered_map<std::string, CliCommand> commandMap = {
        {"vip", CliCommand::Vip},   {"depart", CliCommand::Depart},
        {"stop", CliCommand::Stop}, {"help", CliCommand::Help},
        {"exit", CliCommand::Exit}, {"quit", CliCommand::Exit}};

    auto it = commandMap.find(cmd);
    if (it != commandMap.end()) {
      return it->second;
    }
    return CliCommand::Unknown;
  }
};
