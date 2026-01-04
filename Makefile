BUILD_DIR = build
LOG_DIR = logs
EXECUTABLE = main
SOURCES := $(shell find src include tests -name '*.cpp' -o -name '*.h')

export CC=clang
export CXX=clang++

GREEN  := \033[32m
CYAN   := \033[36m
YELLOW := \033[33m
RESET  := \033[0m

.PHONY: all build clean run rebuild help

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LOG_DIR)
	@echo -e "$(CYAN)[info] Configuring CMake$(RESET)"
	@cd $(BUILD_DIR) && cmake ..
	@echo -e "$(CYAN)[info] Compiling project$(RESET)"
	@cd $(BUILD_DIR) && $(MAKE) --no-print-directory
	@echo -e "$(GREEN)[success] Build complete.$(RESET)"

run: build
	@echo -e "$(GREEN)[info] Starting automated simulation$(RESET)"
	@chmod +x run.sh
	@./run.sh

ipc:
	@echo -e "$(CYAN)[info] Current IPC Resources:$(RESET)"
	@ipcs -m -s -q | grep $(shell whoami)

test: build
	@echo -e "$(GREEN)[info] Running tests$(RESET)"
	@cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	@echo -e "$(YELLOW)[info] Cleaning build directory and logs. $(RESET)"
	@rm -rf $(BUILD_DIR)
	@rm -rf $(LOG_DIR)/*
	@echo -e "$(GREEN)[success] Clean complete.$(RESET)"

format:
	@echo -e "$(CYAN)[info] Formatting code$(RESET)"
	@clang-format -i $(SOURCES)
	@echo -e "$(GREEN)[success] Code formatted.$(RESET)"

lint:
	@echo -e "$(CYAN)[info] Checking code formatting$(RESET)"
	@clang-format --dry-run --Werror $(SOURCES)
	@echo -e "$(GREEN)[success] Code style is correct.$(RESET)"

rebuild: clean build

help:
	@echo -e "$(CYAN)Available commands:$(RESET)"
	@echo "  make         - Build the project incrementally"
	@echo "  make run     - Build and execute the simulation"
	@echo "  make clean   - Remove build artifacts and logs"
	@echo "  make rebuild - Clean and build from scratch"

docs:
	@echo -e "$(CYAN)[info] Generating documentation.$(RESET)"
	@mkdir -p docs
	@doxygen Doxyfile
	@echo -e "$(GREEN)[success] Documentation generated in docs/html/index.html$(RESET)"
