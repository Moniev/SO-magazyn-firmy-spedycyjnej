#pragma once

#include <string>
#include <unordered_map>

enum class CliCommand { Unknown, Vip, Depart, Stop, Help, Exit };

class CommandResolver {
public:
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
