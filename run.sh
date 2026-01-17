#!/bin/bash

CYAN='\033[36m'
GREEN='\033[32m'
YELLOW='\033[33m'
RESET='\033[0m'

remove_ipc() {
  echo -e "${YELLOW}[run.sh] Ensuring clean slate (removing old IPC)...${RESET}"
  ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -m 2>/dev/null
  ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -s 2>/dev/null
  ipcs -q | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -q 2>/dev/null
}

remove_ipc

export LOG_LEVEL="info"
export LOG_TO_CONSOLE="true"
export LOG_TO_FILE="true"
export BELT_SPEED_MS="1000"

if [ ! -f "./build/main" ]; then
  echo -e "${CYAN}[error] Binary ./build/main not found! Run 'make build' first.${RESET}"
  exit 1
fi

echo -e "${CYAN}[run.sh] Starting Warehouse Orchestrator...${RESET}"
echo -e "${CYAN}[run.sh] Logs will be saved to ./logs/${RESET}"

./build/main

echo -e "${GREEN}[run.sh] Simulation finished.${RESET}"
