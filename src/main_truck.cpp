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
