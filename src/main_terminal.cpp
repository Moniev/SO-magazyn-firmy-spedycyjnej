#include "../include/Manager.h"
#include "../include/terminal/TerminalManager.h"
#include "spdlog/spdlog.h"

int main() {
  Manager manager(false);

  if (!manager.session_store->login(
          "AdminConsole", UserRole::Operator | UserRole::SysAdmin, 1, 1)) {
    spdlog::critical("[terminal manager] failed to run terminal manager");

    return 1;
  }

  TerminalManager terminal(&manager);
  spdlog::info("[terminal manager] VIP Handler ready.");

  terminal.run();

  manager.session_store->logout();
  return 0;
}
