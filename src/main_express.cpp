/**
 * @file dispatcher_main.cpp
 * @brief Consumer process that routes packages from the belt to the truck.
 * * Uses the Dispatcher component to block on the belt (waiting for items)
 * and then synchronize with the dock to perform the transfer.
 */

#include "../include/Express.h"
#include "../include/Manager.h"

int main() {
  Manager manager(false);

  if (!manager.session_store->login("System-Express", UserRole::Operator, 0,
                                    1)) {
    return 1;
  }

  Express express(
      manager.getState(), [&]() { manager.lockDock(); },
      [&]() { manager.unlockDock(); }, [&]() { manager.lockBelt(); },
      [&]() { manager.unlockBelt(); },
      [&](SignalType s) { manager.sendSignal(s); });

  spdlog::info("[express] VIP Handler ready.");

  while (manager.getState()->running) {
    SignalType sig = manager.receiveSignalBlocking();

    if (sig == SIGNAL_EXPRESS_LOAD) {
      express.deliverVipPackage();
    } else if (sig == SIGNAL_END_WORK) {
      break;
    }
  }

  manager.session_store->logout();
  return 0;
}
