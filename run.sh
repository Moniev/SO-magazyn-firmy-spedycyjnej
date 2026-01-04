#!/bin/bash

CYAN='\033[36m'
GREEN='\033[32m'
RESET='\033[0m'

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

echo -e "${GREEN}[sim] All of processes are currently runing.${RESET}"
echo "Press Ctrl+C, to stop whole simulation"

cleanup() {
  echo -e "\n${CYAN}[sim] Closing processes.${RESET}"
  kill $BELT_PID $DISP_PID $TRUCK_PID $EXPR_PID $MASTER_PID 2>/dev/null
  echo -e "${GREEN}[sim] Simulation closed.${RESET}"
  exit
}

trap cleanup SIGINT

wait
