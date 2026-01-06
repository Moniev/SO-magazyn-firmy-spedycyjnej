/**
 * @file express_main.cpp
 * @brief High-priority worker reacting to SIGNAL_EXPRESS_LOAD.
 * * Unlike other workers, this process blocks on the Message Queue. When a
 * VIP signal is intercepted, it executes a direct-to-truck load, bypassing
 * the standard conveyor belt sequence.
 */

#include "../include/Manager.h"

int main() {
  Manager manager(false);

  if (!manager.session_store->login("System-TruckPool", UserRole::Operator, 0,
                                    5)) {
    return 1;
  }

  manager.truck->run();

  manager.session_store->logout();
  return 0;
}
