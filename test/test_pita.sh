#!/bin/bash

# Simple test script that runs pita_bytes with different arguments
# and displays the outputs

PROGRAM="../bin/pita_bytes"

# Function to run a command and display its output
run_cmd() {
    local cmd="$1"
    echo "=========================================="
    echo "COMMAND: $cmd"
    echo "==================== OUTPUT =============="
    eval "$cmd"
    echo "=========================================="
    echo ""
}

# Basic tests
run_cmd "$PROGRAM"
run_cmd "$PROGRAM -t"
run_cmd "$PROGRAM -t 0"
run_cmd "$PROGRAM -t0"
run_cmd "$PROGRAM -t 10"
run_cmd "$PROGRAM -t -5"
run_cmd "$PROGRAM -t abc"
run_cmd "$PROGRAM -t 8*"
run_cmd "$PROGRAM -t 8abc"
run_cmd "$PROGRAM -t 999999999999999"
run_cmd "$PROGRAM --tables=15"

