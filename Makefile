BUILD_DIR  = build
LOG_DIR    = logs
DOC_DIR    = docs
Doxyfile   = Doxyfile
SOURCES    := $(shell find src include tests -name '*.cpp' -o -name '*.h')

export CC  = clang
export CXX = clang++

GREEN  := \033[32m
CYAN   := \033[36m
YELLOW := \033[33m
RED    := \033[31m
RESET  := \033[0m

.PHONY: all build clean run ipc test format lint rebuild docs help

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LOG_DIR)
	@echo -e "$(CYAN)[info] Configuring CMake...$(RESET)"
	@cd $(BUILD_DIR) && cmake ..
	@echo -e "$(CYAN)[info] Compiling project binaries...$(RESET)"
	@cd $(BUILD_DIR) && $(MAKE) --no-print-directory
	@echo -e "$(GREEN)[success] Build complete.$(RESET)"

run: build
	@echo -e "$(GREEN)[info] Starting automated simulation...$(RESET)"
	@chmod +x run.sh
	@./run.sh

ipc:
	@echo -e "$(CYAN)[info] Current IPC Resources for $(shell whoami):$(RESET)"
	@ipcs -m -s -q | grep $(shell whoami) || echo -e "$(YELLOW)No active IPC resources found.$(RESET)"

test: build
	@echo -e "$(GREEN)[info] Running unit and integration tests...$(RESET)"
	@cd $(BUILD_DIR) && ctest --output-on-failure

docs:
	@if [ ! -f $(Doxyfile) ]; then \
		echo -e "$(RED)[error] $(Doxyfile) not found! Run 'doxygen -g' first.$(RESET)"; \
		exit 1; \
	fi
	@echo -e "$(CYAN)[info] Generating Doxygen documentation...$(RESET)"
	@mkdir -p $(DOC_DIR)
	@doxygen $(Doxyfile)
	@echo -e "$(GREEN)[success] Documentation generated in $(DOC_DIR)/html/index.html$(RESET)"
	@echo -e "$(YELLOW)[info] Opening browser...$(RESET)"
	@xdg-open $(DOC_DIR)/html/index.html &

format:
	@echo -e "$(CYAN)[info] Formatting code...$(RESET)"
	@clang-format -i $(SOURCES)
	@echo -e "$(GREEN)[success] Code formatted.$(RESET)"

lint:
	@echo -e "$(CYAN)[info] Checking code formatting...$(RESET)"
	@clang-format --dry-run --Werror $(SOURCES)
	@echo -e "$(GREEN)[success] Code style is correct.$(RESET)"

clean:
	@echo -e "$(YELLOW)[info] Cleaning build directory, logs and docs...$(RESET)"
	@rm -rf $(BUILD_DIR)
	@rm -rf $(DOC_DIR)
	@rm -rf $(LOG_DIR)/*
	@echo -e "$(GREEN)[success] Clean complete.$(RESET)"

rebuild: clean build

help:
	@echo -e "$(CYAN)Warehouse IPC System - Command Center$(RESET)"
	@echo "---------------------------------------------------"
	@echo -e "$(GREEN)make$(RESET)         - Build the project (CMake)"
	@echo -e "$(GREEN)make run$(RESET)     - Build and execute simulation (run.sh)"
	@echo -e "$(GREEN)make test$(RESET)    - Run GTest/CTest suite"
	@echo -e "$(GREEN)make docs$(RESET)    - Generate and open Doxygen HTML"
	@echo -e "$(GREEN)make ipc$(RESET)     - Show active SHM/SEM/MSG resources"
	@echo -e "$(GREEN)make format$(RESET)  - Apply clang-format to all files"
	@echo -e "$(GREEN)make clean$(RESET)   - Wipe build, logs and docs"
	@echo -e "$(GREEN)make rebuild$(RESET) - Clean and build from scratch"
