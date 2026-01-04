#ifndef SHARED_SPECS_H
#define SHARED_SPECS_H

#include <cstdint>
#include <ctime>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>

constexpr int MAX_BELT_CAPACITY_K = 10;
constexpr double MAX_BELT_WEIGHT_M = 100.0;

constexpr double VOL_A = 19.5;
constexpr double VOL_B = 46.2;
constexpr double VOL_C = 99.7;

constexpr int SHM_KEY_ID = 1234;
constexpr int SEM_KEY_ID = 5678;
constexpr int MSG_KEY_ID = 9012;

constexpr int MAX_PACKAGE_HISTORY = 6;
constexpr int MAX_USERS_SESSIONS = 5;

using OrgId = int;

enum SemIndex {
  SEM_MUTEX_BELT = 0,
  SEM_EMPTY_SLOTS,
  SEM_FULL_SLOTS,
  SEM_DOCK_MUTEX,
  SEM_TOTAL
};

enum SignalType {
  SIGNAL_NONE = 0,
  SIGNAL_DEPARTURE = 1,
  SIGNAL_EXPRESS_LOAD = 2,
  SIGNAL_END_WORK = 3
};

enum class PackageType : unsigned char {
  None = 0,
  TypeA = 1 << 0,
  TypeB = 1 << 1,
  TypeC = 1 << 2,

  All = TypeA | TypeB | TypeC
};

inline constexpr PackageType operator|(PackageType lhs, PackageType rhs) {
  return static_cast<PackageType>(static_cast<unsigned char>(lhs) |
                                  static_cast<unsigned char>(rhs));
}

inline constexpr PackageType operator&(PackageType lhs, PackageType rhs) {
  return static_cast<PackageType>(static_cast<unsigned char>(lhs) &
                                  static_cast<unsigned char>(rhs));
}

inline constexpr bool hasFlag(PackageType value, PackageType flag) {
  return (static_cast<unsigned char>(value) &
          static_cast<unsigned char>(flag)) != 0;
}

enum class UserRole : uint16_t {
  None = 0,
  Viewer = 1 << 0,
  Operator = 1 << 1,
  OrgAdmin = 1 << 2,
  SysAdmin = 1 << 3,

  All = Viewer | Operator | OrgAdmin | SysAdmin
};

inline constexpr UserRole operator|(UserRole lhs, UserRole rhs) {
  return static_cast<UserRole>(static_cast<uint16_t>(lhs) |
                               static_cast<uint16_t>(rhs));
}

inline constexpr UserRole operator&(UserRole lhs, UserRole rhs) {
  return static_cast<UserRole>(static_cast<uint16_t>(lhs) &
                               static_cast<uint16_t>(rhs));
}

inline constexpr bool hasFlag(UserRole value, UserRole flag) {
  return (static_cast<uint16_t>(value) & static_cast<uint16_t>(flag)) != 0;
}

struct UserContext {
  std::string username;
  UserRole role;
  OrgId orgId;

  UserContext() : username("anonymous"), role(UserRole::None), orgId(0) {}

  UserContext(std::string u, UserRole r, OrgId o)
      : username(std::move(u)), role(r), orgId(o) {}
};

enum class PackageStatus : unsigned char {
  Normal = 0,
  Express = 1 << 0,
  Loaded = 1 << 1
};

inline constexpr PackageStatus operator|(PackageStatus lhs, PackageStatus rhs) {
  return static_cast<PackageStatus>(static_cast<unsigned char>(lhs) |
                                    static_cast<unsigned char>(rhs));
}

inline constexpr PackageStatus operator&(PackageStatus lhs, PackageStatus rhs) {
  return static_cast<PackageStatus>(static_cast<unsigned char>(lhs) &
                                    static_cast<unsigned char>(rhs));
}

inline constexpr bool hasFlag(PackageStatus value, PackageStatus flag) {
  return (static_cast<unsigned char>(value) &
          static_cast<unsigned char>(flag)) != 0;
}

enum class ActionType : uint8_t {
  None = 0,

  Created = 1 << 0,
  PlacedOnBelt = 1 << 1,
  PickedUp = 1 << 2,
  LoadedToTruck = 1 << 3,

  ByWorker = 1 << 4,
  ByExpress = 1 << 5,
  ByTruck = 1 << 6,
  Forced = 1 << 7
};

inline constexpr ActionType operator|(ActionType lhs, ActionType rhs) {
  return static_cast<ActionType>(static_cast<uint8_t>(lhs) |
                                 static_cast<uint8_t>(rhs));
}

inline constexpr ActionType operator&(ActionType lhs, ActionType rhs) {
  return static_cast<ActionType>(static_cast<uint8_t>(lhs) &
                                 static_cast<uint8_t>(rhs));
}

inline constexpr bool hasFlag(ActionType value, ActionType flag) {
  return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

struct ActionRecord {
  ActionType type;
  pid_t actor_pid;
  time_t timestamp;
};

struct Package {
  int id;

  pid_t creator_pid;
  pid_t editor_pid;

  PackageType type;
  PackageStatus status;

  double weight;
  double volume;

  time_t created_at;
  time_t updated_at;

  ActionRecord history[MAX_PACKAGE_HISTORY];
  int history_count;

  void pushAction(ActionType action, pid_t pid) {
    if (history_count < MAX_PACKAGE_HISTORY) {
      history[history_count] = {action, pid, std::time(nullptr)};
      history_count++;

      updated_at = std::time(nullptr);
      editor_pid = pid;
    }
  }
};

struct UserSession {
  bool active;
  char username[32];
  pid_t session_pid;

  UserRole role;
  OrgId orgId;

  int max_processes;
  int current_processes;
};

struct TruckState {
  bool is_present;
  int id;
  int current_load;
  int max_load;
  double current_weight;
  double max_weight;
};

struct SharedState {
  Package belt[MAX_BELT_CAPACITY_K];
  int head;
  int tail;

  int current_items_count;
  double current_belt_weight;

  bool running;
  int trucks_completed;
  int total_packages_created;

  bool force_truck_departure;
  bool p4_load_command;
  UserSession users[MAX_USERS_SESSIONS];
  TruckState dock_truck;
};

struct CommandMessage {
  long mtype;
  int command_id;
};

#endif
