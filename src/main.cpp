/**
 * @file main.cpp
 * @brief Master Orchestrator - Replaces run.sh functionality.
 * * Initializes IPC resources using Manager(true).
 * * Spawns worker processes using fork() and execv().
 * * Monitors child processes and handles clean shutdown (SIGINT).
 */
#include "../include/Config.h"
#include "../include/Manager.h"
#include <csignal>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

volatile std::sig_atomic_t stop_requested = 0;
std::vector<pid_t> children_pids;

/**
 * @brief Signal handler for Ctrl+C (SIGINT).
 * Sets the flag to break the main loop.
 */
void handleSigint(int) { stop_requested = 1; }

/**
 * @brief Spawns a child process using fork/exec pattern.
 * @param binary_path Relative or absolute path to the executable.
 * @param proc_name Name of the process (argv[0]).
 * @param arg Optional argument (argv[1]), e.g., Worker ID. Default is empty.
 */
void spawnChild(const std::string &binary_path, const std::string &proc_name,
                const std::string &arg = "") {
  pid_t pid = fork();

  if (pid < 0) {
    spdlog::critical("[master] Failed to fork process: {}", proc_name);
    exit(1);
  }

  if (pid == 0) {
    std::vector<char *> args;

    args.push_back(const_cast<char *>(proc_name.c_str()));

    if (!arg.empty()) {
      args.push_back(const_cast<char *>(arg.c_str()));
    }

    args.push_back(nullptr);

    execv(binary_path.c_str(), args.data());

    perror("execv failed");
    exit(1);
  } else {
    if (arg.empty()) {
      spdlog::info("[master] Spawned {} (PID: {})", proc_name, pid);
    } else {
      spdlog::info("[master] Spawned {} with ID {} (PID: {})", proc_name, arg,
                   pid);
    }
    children_pids.push_back(pid);
  }
}

int main() {
  std::signal(SIGINT, handleSigint);

  if (!std::filesystem::exists("logs")) {
    std::filesystem::create_directory("logs");
  }

  std::remove("logs/simulation_report.txt");

  Config::get().setupLogger("system-master");
  spdlog::info(
      "[master] Starting Warehouse Orchestrator with Fleet Support...");

  Manager manager(true);

  spawnChild("./build/dispatcher", "dispatcher");
  spawnChild("./build/express", "express");
  spawnChild("./build/belt", "belt");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  int num_trucks = 3;
  for (int i = 1; i <= num_trucks; ++i) {
    spawnChild("./build/truck", "truck", std::to_string(i));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  for (int i = 1; i <= 3; ++i) {
    spawnChild("./build/worker", "worker", std::to_string(i));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  spdlog::info(
      "[master] Deployment complete. {} trucks in the pool. Monitoring...",
      num_trucks);

  while (!stop_requested && manager.getState()->running) {
    int status;
    pid_t dead_pid = waitpid(-1, &status, WNOHANG);

    if (dead_pid > 0) {
      spdlog::warn(
          "[master] Process PID {} died. Check logs for stability issues.",
          dead_pid);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  spdlog::warn(
      "[master] Shutdown signal received. Terminating all processes...");
  manager.getState()->running = false;

  for (pid_t pid : children_pids) {
    kill(pid, SIGTERM);
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  spdlog::info(
      "[master] IPC resources marked for destruction. System offline.");

  return 0;
}
