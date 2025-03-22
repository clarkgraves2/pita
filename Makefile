# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Waggregate-return -Wwrite-strings -Wvla -Wfloat-equal -Wstack-usage=1024 -Werror
DEBUG_FLAGS = -g

# Valgrind settings
VALGRIND = valgrind
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

# Directories
SRC_DIR = src
BIN_DIR = bin
TEST_DIR = tests
TEST_RESULTS_DIR = $(TEST_DIR)/results

# Source and target
SRC = $(SRC_DIR)/server_main.c \
      $(SRC_DIR)/cmd_line_opts.c \
	  $(SRC_DIR)/syslog.c
TARGET = $(BIN_DIR)/pita_bytes

# Test file - can be overridden with make valgrind_tests TEST_FILE=your_file.txt
TEST_FILE = $(TEST_DIR)/tests.txt

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TEST_RESULTS_DIR):
	mkdir -p $(TEST_RESULTS_DIR)

# Build the server (regular build)
$(TARGET): $(SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Build with debug symbols for Valgrind
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Run Valgrind with default options or custom args via ARGS="your args"
valgrind: debug
	$(VALGRIND) $(VALGRIND_FLAGS) $(TARGET) $(ARGS)

# Valgrind batch testing - reads options from test file
valgrind_tests: debug
	@echo "Running Valgrind tests, saving to results.txt..."
	@echo "Valgrind Test Results - $(shell date)" > results.txt
	@bash -c 'grep -v "^#" ./tests/tests.txt | grep -v "^\s*$$" | while read line; do \
		echo "\n==== Testing: $$line ====" >> results.txt; \
		$(VALGRIND) $(VALGRIND_FLAGS) $(TARGET) $$line 2>&1 >> results.txt; \
	done'
	@echo "Tests completed. Results saved to results.txt"

# Create test directory if it doesn't exist
$(TEST_DIR):
	mkdir -p $(TEST_DIR)

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)
	rm -rf $(TEST_RESULTS_DIR)
	rm -f ./server.log
	rm -f ./results.txt

# Set as the default target
.DEFAULT_GOAL := $(TARGET)

# Declare phony targets
.PHONY: clean debug valgrind valgrind_tests