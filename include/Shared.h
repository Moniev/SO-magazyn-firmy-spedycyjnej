/**
 * @file Shared.h
 * @brief Global specifications, constants, and data structures for IPC Shared
 * Memory.
 * * This header defines the exact memory layout used by all processes to
 * interpret the raw bytes in the Shared Memory segment. It includes
 * configuration constants, bitmask enumerations for state management, and the
 * core data structures.
 */

#ifndef SHARED_H
#define SHARED_H

#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

/** @name Warehouse Capacity Constraints
 * Constants defining physical and logical limits of the conveyor belt.
 * @{ */
constexpr int MAX_BELT_CAPACITY_K =
    10; /**< Maximum number of slots in the circular buffer. */
constexpr double MAX_BELT_WEIGHT_M =
    100.0; /**< Maximum total weight allowed on the belt. */
/** @} */

/** @name Package Volume Constants
 * Standardized volumes for different package types.
 * @{ */
constexpr double VOL_A = 19.5; /**< Standard volume for Type A packages. */
constexpr double VOL_B = 46.2; /**< Standard volume for Type B packages. */
constexpr double VOL_C = 99.7; /**< Standard volume for Type C packages. */
/** @} */

/** @name IPC Identification Keys
 * Magic numbers used to generate System V IPC keys via ftok or direct
 * assignment.
 * @{ */
constexpr int SHM_KEY_ID =
    1234; /**< Key for Shared Memory segment allocation. */
constexpr int SEM_KEY_ID = 5678; /**< Key for Semaphore Set allocation. */
constexpr int MSG_KEY_ID = 9012; /**< Key for Message Queue allocation. */
/** @} */

/** @name System Limits */
/** @{ */
constexpr int MAX_PACKAGE_HISTORY =
    6; /**< Maximum number of audit entries per package. */
constexpr int MAX_USERS_SESSIONS =
    5; /**< Maximum number of concurrent process sessions. */
/** @} */

/** @brief Type alias for Organization Identifier. */
using OrgId = int;

/**
 * @enum SemIndex
 * @brief Mapping of semaphore indices within the system semaphore set.
 */
enum SemIndex {
  SEM_MUTEX_BELT =
      0, /**< Binary semaphore (Mutex) protecting belt structural integrity. */
  SEM_EMPTY_SLOTS, /**< Counting semaphore tracks available space (Producer
                      wait). */
  SEM_FULL_SLOTS,  /**< Counting semaphore tracks available items (Consumer
                      wait). */
  SEM_DOCK_MUTEX,  /**< Binary semaphore (Mutex) protecting dock/truck state. */
  SEM_TOTAL        /**< Total number of semaphores in the set. */
};

/**
 * @enum SignalType
 * @brief Commands sent via the System V Message Queue.
 */
enum SignalType {
  SIGNAL_NONE = 0,         /**< No operation / Invalid signal. */
  SIGNAL_DEPARTURE = 1,    /**< Forces the truck to depart immediately. */
  SIGNAL_EXPRESS_LOAD = 2, /**< Triggers a VIP package generation sequence. */
  SIGNAL_END_WORK = 3 /**< Signals all processes to terminate gracefully. */
};

/**
 * @enum PackageType
 * @brief Bitmask classification of package contents.
 */
enum class PackageType : unsigned char {
  None = 0,
  TypeA = 1 << 0, /**< Small items. */
  TypeB = 1 << 1, /**< Medium items. */
  TypeC = 1 << 2, /**< Large items. */

  All = TypeA | TypeB | TypeC
};

/** @name PackageType Bitwise Operators
 * Helper functions to enable bitmask operations on PackageType.
 * @{ */
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
/** @} */

/**
 * @enum UserRole
 * @brief Role-Based Access Control (RBAC) permissions bitmask.
 */
enum class UserRole : uint16_t {
  None = 0,
  Viewer = 1 << 0,   /**< Read-only access to stats. */
  Operator = 1 << 1, /**< Can control belt and truck operations. */
  OrgAdmin = 1 << 2, /**< Organization-level management. */
  SysAdmin = 1 << 3, /**< Full system control (including shutdown). */

  All = Viewer | Operator | OrgAdmin | SysAdmin
};

/** @name UserRole Bitwise Operators
 * Helper functions to enable bitmask operations on UserRole.
 * @{ */
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
/** @} */

/**
 * @struct UserContext
 * @brief Local helper structure for passing user credentials within the
 * application.
 * @note This structure is NOT stored in Shared Memory (uses std::string).
 */
struct UserContext {
  std::string username;
  UserRole role;
  OrgId orgId;

  UserContext() : username("anonymous"), role(UserRole::None), orgId(0) {}

  UserContext(std::string u, UserRole r, OrgId o)
      : username(std::move(u)), role(r), orgId(o) {}
};

/**
 * @enum PackageStatus
 * @brief Lifecycle state tracking for a package.
 */
enum class PackageStatus : unsigned char {
  Normal = 0,       /**< Standard processing. */
  Express = 1 << 0, /**< High priority (VIP). */
  Loaded = 1 << 1   /**< Successfully loaded onto a truck. */
};

/** @name PackageStatus Bitwise Operators
 * Helper functions to enable bitmask operations on PackageStatus.
 * @{ */
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
/** @} */

/**
 * @enum ActionType
 * @brief Event types for the package audit trail.
 */
enum class ActionType : uint8_t {
  None = 0,

  Created = 1 << 0,       /**< Package generated. */
  PlacedOnBelt = 1 << 1,  /**< Entered circular buffer. */
  PickedUp = 1 << 2,      /**< Removed from circular buffer. */
  LoadedToTruck = 1 << 3, /**< Finalized in truck. */

  ByWorker = 1 << 4,  /**< Action performed by standard process. */
  ByExpress = 1 << 5, /**< Action performed by Express/VIP process. */
  ByTruck = 1 << 6,   /**< Action performed by Logistics logic. */
  Forced = 1 << 7     /**< Action forced (e.g., manual override). */
};

/** @name ActionType Bitwise Operators
 * Helper functions to enable bitmask operations on ActionType.
 * @{ */
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
/** @} */

/**
 * @struct ActionRecord
 * @brief A single entry in the package's history log.
 */
struct ActionRecord {
  ActionType type;  /**< Type of action performed. */
  pid_t actor_pid;  /**< PID of the process performing the action. */
  time_t timestamp; /**< Time of the event. */
};

/**
 * @struct Package
 * @brief The core unit of data in the system.
 * * Represents a physical package moving through the warehouse.
 * Includes properties and a fixed-size history buffer for audit trails.
 */
struct Package {
  int id; /**< Global Unique ID. */

  pid_t creator_pid; /**< Process ID that created this package. */
  pid_t editor_pid;  /**< Process ID that last modified this package. */

  PackageType type;     /**< Physical classification (A, B, C). */
  PackageStatus status; /**< Current status flags. */

  double weight; /**< Weight in kg. */
  double volume; /**< Volume in arbitrary units. */

  time_t created_at; /**< Creation timestamp. */
  time_t updated_at; /**< Last modification timestamp. */

  ActionRecord
      history[MAX_PACKAGE_HISTORY]; /**< Circular buffer of history events. */
  int history_count;                /**< Current number of history records. */

  /**
   * @brief Appends a new action to the package's history.
   * @param action Bitmask of the action performed.
   * @param pid PID of the actor.
   */
  void pushAction(ActionType action, pid_t pid) {
    if (history_count < MAX_PACKAGE_HISTORY) {
      history[history_count] = {action, pid, std::time(nullptr)};
      history_count++;

      updated_at = std::time(nullptr);
      editor_pid = pid;
    }
  }
};

/**
 * @struct UserSession
 * @brief Registry entry for an active process session.
 * * Stored in the SharedState::users array. Tracks who is connected to the
 * system.
 */
struct UserSession {
  bool active;       /**< True if slot is occupied. */
  char username[32]; /**< Username (C-string). */
  pid_t session_pid; /**< PID of the main process for this session. */

  UserRole role; /**< Access permissions. */
  OrgId orgId;   /**< Organization ID. */

  int max_processes;     /**< Quota: max concurrent sub-processes allowed. */
  int current_processes; /**< Current number of running sub-processes. */
};

/**
 * @struct TruckState
 * @brief Represents the vehicle currently stationed at the dock.
 */
struct TruckState {
  bool is_present;       /**< True if a truck is physically at the dock. */
  int id;                /**< Unique Truck ID. */
  int current_load;      /**< Number of packages currently loaded. */
  int max_load;          /**< Maximum package capacity. */
  double current_weight; /**< Current total weight loaded. */
  double max_weight;     /**< Maximum weight capacity. */
};

/**
 * @struct SharedState
 * @brief The master memory map for the IPC Shared Memory segment.
 *
 * * This structure is mapped at the same offset in all processes. It contains
 * the circular buffer (belt), truck dock state, and user session registry.
 */
struct SharedState {
  Package belt[MAX_BELT_CAPACITY_K]; /**< Circular buffer for packages. */
  int head;                          /**< Consumer index (Read/Pop). */
  int tail;                          /**< Producer index (Write/Push). */

  int current_items_count;    /**< Atomic-like counter of items on belt. */
  double current_belt_weight; /**< Total weight on the belt. */

  bool running;               /**< System run-loop flag. */
  int trucks_completed;       /**< Statistics: Total trucks departed. */
  int total_packages_created; /**< Global counter for generating Package IDs. */

  bool force_truck_departure; /**< Flag to signal immediate departure. */
  bool p4_load_command;       /**< Legacy/Debug flag. */

  UserSession users[MAX_USERS_SESSIONS]; /**< Table of active sessions. */
  TruckState dock_truck;                 /**< State of the docking bay. */
};

/**
 * @struct CommandMessage
 * @brief Data structure for System V Message Queue operations.
 * * Must follow the specific layout required by msgsnd/msgrcv.
 */
struct CommandMessage {
  long mtype;     /**< Message type (must be > 0). */
  int command_id; /**< Payload (cast to SignalType). */
};

#endif
