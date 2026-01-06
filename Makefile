BUILD_DIR  = build
LOG_DIR    = logs
DOC_DIR    = docs
DOCKER_DIR = docker
Doxyfile   = Doxyfile
SOURCES    := $(shell find src include test -name '*.cpp' -o -name '*.h')
PACKAGE_NAME = warehouse-ipc-linux-x64.tar.gz

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

docker-build:
	@echo -e "$(CYAN)[info] Building Alpine-based Docker image...$(RESET)"
	@docker compose -f $(DOCKER_DIR)/docker-compose.yml build

docker-run:
	@echo -e "$(GREEN)[info] Launching simulation in Alpine container...$(RESET)"
	@docker compose -f $(DOCKER_DIR)/docker-compose.yml up

docker-test:
	@echo -e "$(CYAN)[info] Running tests inside Docker container...$(RESET)"
	@docker compose -f $(DOCKER_DIR)/docker-compose.yml run --rm warehouse-sim make test

docker-clean:
	@echo -e "$(YELLOW)[info] Removing Docker artifacts...$(RESET)"
	@docker compose -f $(DOCKER_DIR)/docker-compose.yml down --rmi all

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

package: build
	@echo -e "$(CYAN)[info] Packaging binaries into $(PACKAGE_NAME)...$(RESET)"
	@tar -czf $(PACKAGE_NAME) \
		-C $(BUILD_DIR) master belt_main dispatcher_main express_main truck_main \
		-C .. run.sh README.md
	@echo -e "$(GREEN)[success] Package ready: $(PACKAGE_NAME)$0"

help:
	@echo -e "$(CYAN)Warehouse IPC System - Command Center$(RESET)"
	@echo "---------------------------------------------------"
	@echo -e "$(YELLOW)Local Commands:$(RESET)"
	@echo "  make             - Build the project"
	@echo "  make run         - Execute simulation locally"
	@echo "  make test        - Run tests locally"
	@echo "  make docs        - Generate & open Doxygen"
	@echo "  make ipc         - Show active Linux IPC resources"
	@echo ""
	@echo -e "$(YELLOW)Docker Commands (Alpine):$(RESET)"
	@echo "  make docker-build - Build Alpine Docker image"
	@echo "  make docker-run   - Run simulation in Docker"
	@echo "  make docker-test  - Run tests inside Docker"
	@echo "  make docker-clean - Remove Docker containers/images"
	@echo ""
	@echo -e "$(YELLOW)Maintenance:$(RESET)"
	@echo "  make format      - Apply clang-format"
	@echo "  make clean       - Wipe everything"
