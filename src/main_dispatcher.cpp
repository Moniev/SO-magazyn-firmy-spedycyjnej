#include "../include/Manager.h"

int main() {
  Manager manager(false);

  if (!manager.session_store->login("System-Dispatcher", UserRole::Operator, 0,
                                    1)) {
    return 1;
  }

  spdlog::info("[dispatcher] Ready to route packages.");

  while (manager.getState()->running) {
    manager.dispatcher->processNextPackage();
  }

  manager.session_store->logout();
  return 0;
}
