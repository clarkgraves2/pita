# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Waggregate-return -Wwrite-strings -Wvla -Wfloat-equal -Wstack-usage=1024 -Werror

# Directories
SRC_DIR = src
BIN_DIR = bin

# Source and target
SRC = $(SRC_DIR)/server_main.c
TARGET = $(BIN_DIR)/pita_bytes

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build the server
$(TARGET): $(SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)

# Set as the default target
.DEFAULT_GOAL := $(TARGET)

# Declare phony targets
.PHONY: clean run run_tables run_help