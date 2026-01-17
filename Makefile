BUILD_DIR  = build
LOG_DIR    = logs
DOC_DIR    = docs
DOCKER_DIR = docker
Doxyfile   = Doxyfile
SOURCES    := $(shell find src include test -name '*.cpp' -o -name '*.h')
PACKAGE_NAME = warehouse-ipc-linux-x64.tar.gz
CLANG_FORMAT := /usr/bin/clang-format

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

terminal:
	@ipcs -m | grep $(shell whoami) > /dev/null || (echo -e "$(RED)[error] Simulation not running! Run 'make run' first.$(RESET)" && exit 1)
	@echo -e "$(CYAN)[info] Connecting to Warehouse Terminal...$(RESET)"
	@./$(BUILD_DIR)/terminal

ipc:
	@echo -e "$(CYAN)[info] Current IPC Resources for $(shell whoami):$(RESET)"
	@ipcs -m -s -q | grep $(shell whoami) || echo -e "$(YELLOW)No active IPC resources found.$(RESET)"

ipc-clean:
	@echo -e "$(YELLOW)[info] Cleaning up stale IPC resources...$(RESET)"
	@ipcs -m | grep $(shell whoami) | awk '{print $$2}' | xargs -r ipcrm -m
	@ipcs -s | grep $(shell whoami) | awk '{print $$2}' | xargs -r ipcrm -s
	@ipcs -q | grep $(shell whoami) | awk '{print $$2}' | xargs -r ipcrm -q
	@echo -e "$(GREEN)[success] IPC resources cleared.$(RESET)"

ipc-fclean:
	@echo -e "$(RED)[danger] Cleaning ALL system IPC resources with sudo...$(RESET)"
	@sudo ipcs -m | awk 'NR>3 {print $$2}' | xargs -r -n1 sudo ipcrm -m
	@sudo ipcs -s | awk 'NR>3 {print $$2}' | xargs -r -n1 sudo ipcrm -s
	@sudo ipcs -q | awk 'NR>3 {print $$2}' | xargs -r -n1 sudo ipcrm -q
	@echo -e "$(GREEN)[success] All system IPC resources have been wiped.$(RESET)"

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

docker-terminal:
	@echo -e "$(CYAN)[info] Attaching to Warehouse Terminal inside Docker...$(RESET)"
	@docker exec -it warehouse_alpine_sim ./build/terminal

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
	@if [ -f "$(CLANG_FORMAT)" ]; then \
		$(CLANG_FORMAT) -i $(SOURCES); \
	else \
		echo -e "$(RED)[error] Not found $(CLANG_FORMAT)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)[success] Code formatted.$(RESET)"

lint:
	@echo -e "$(CYAN)[info] Checking code formatting...$(RESET)"
	@if [ -f "$(CLANG_FORMAT)" ]; then \
		$(CLANG_FORMAT) --dry-run --Werror $(SOURCES); \
	else \
		echo -e "$(RED)[error] Not found $(CLANG_FORMAT).$(RESET)"; \
		exit 1; \
	fi
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
		-C $(BUILD_DIR) main belt dispatcher express truck terminal \
		-C . run.sh README.md
	@echo -e "$(GREEN)[success] Package ready: $(PACKAGE_NAME)$(RESET)"

release-tag:
	@echo -e "$(YELLOW)To release this version, run:$(RESET)"
	@echo "git tag -a v$(VERSION) -m 'Release v$(VERSION)'"
	@echo "git push origin v$(VERSION)"

help:
	@echo -e "$(CYAN)Warehouse IPC System - Command Center$(RESET)"
	@echo ""
	@echo -e "$(YELLOW)Local Commands:$(RESET)"
	@echo "  make             - Build the project binaries"
	@echo "  make run         - Execute simulation (background workers)"
	@echo "  make terminal    - Open interactive console (attach to running sim)"
	@echo "  make test        - Run GTest/CTest suite locally"
	@echo ""
	@echo -e "$(YELLOW)Docker Commands (Alpine):$(RESET)"
	@echo "  make docker-build - Build Alpine Linux Docker image"
	@echo "  make docker-run   - Run simulation inside Docker container"
	@echo "  make docker-test  - Run tests inside Docker environment"
	@echo "  make docker-clean - Remove Docker containers and images"
	@echo "  make docker-terminal - Run terminal binary in docker environment"
	@echo ""
	@echo -e "$(YELLOW)IPC Management:$(RESET)"
	@echo "  make ipc         - List active SHM/SEM/MSG resources"
	@echo "  make ipc-clean   - Remove IPC resources owned by $(shell whoami)"
	@echo "  make ipc-fclean  - Force remove ALL system IPC (requires sudo)"
	@echo ""
	@echo -e "$(YELLOW)Maintenance & Distribution:$(RESET)"
	@echo "  make package     - Create .tar.gz bundle for release"
	@echo "  make docs        - Generate and open Doxygen documentation"
	@echo "  make format      - Apply clang-format to all files"
	@echo "  make clean       - Wipe build directory, logs and docs"
	@echo "  make rebuild     - Full clean and build"
