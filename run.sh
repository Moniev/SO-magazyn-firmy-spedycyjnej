#!/bin/bash

CYAN='\033[36m'
GREEN='\033[32m'
RESET='\033[0m'

remove_ipc() {
  echo -e "${YELLOW}[sim] Removing IPC resources for user: $(whoami)...${RESET}"
  ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -m 2>/dev/null
  ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -s 2>/dev/null
  ipcs -q | grep $(whoami) | awk '{print $2}' | xargs -r ipcrm -q 2>/dev/null
}

remove_ipc

echo -e "${CYAN}[sim] Running binaries! ${RESET}"

./build/main &
MASTER_PID=$!
sleep 1

./build/truck &
TRUCK_PID=$!

./build/dispatcher &
DISP_PID=$!

./build/express &
EXPR_PID=$!

sleep 1

./build/belt &
BELT_PID=$!

echo -e "${GREEN}[sim] All of processes are currently running.${RESET}"
echo "Press Ctrl+C, to stop whole simulation"

cleanup() {
  echo -e "\n${CYAN}[sim] Terminating processes...${RESET}"
  kill $BELT_PID $DISP_PID $TRUCK_PID $EXPR_PID $MASTER_PID 2>/dev/null

  sleep 0.5
  remove_ipc

  echo -e "${GREEN}[sim] Simulation closed and IPC resources freed.${RESET}"
  exit
}

trap cleanup SIGINT

wait
