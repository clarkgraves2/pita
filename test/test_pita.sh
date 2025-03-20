#!/bin/bash

# Enhanced test script that runs pita_bytes with different arguments
# and displays the outputs - focusing on the -o option

PROGRAM="../bin/pita_bytes"

# Function to run a command and display its output
run_cmd() {
    local cmd="$1"
    echo "=========================================="
    echo "COMMAND: $cmd"
    echo "==================== OUTPUT =============="
    # Use script to capture all terminal output
    output=$(eval "$cmd" 2>&1)
    status=$?
    echo "$output"
    echo "Result code: $status"
    echo "=========================================="
    echo ""
}

# Make sure the program exists
if [ ! -f "$PROGRAM" ]; then
    echo "Error: Program $PROGRAM not found. Make sure to build it first."
    exit 1
fi

echo "========== TESTING -t OPTION ============"
# Basic tests for -t option
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

echo "========== TESTING -o OPTION ============"
# Basic tests for -o option
run_cmd "$PROGRAM -o"                   # Missing argument
run_cmd "$PROGRAM -o 0800"              # Valid opening hour (8 AM)
run_cmd "$PROGRAM -o0800"               # Valid opening hour without space
run_cmd "$PROGRAM -o 800"               # Valid opening hour without leading zero

# Invalid format tests
run_cmd "$PROGRAM -o abc"               # Non-numeric
run_cmd "$PROGRAM -o 8a00"              # Contains letters
run_cmd "$PROGRAM -o 08:00"             # Incorrect format with colon
run_cmd "$PROGRAM -o 8:00"              # Incorrect format with colon

# Time range tests
run_cmd "$PROGRAM -o -100"              # Negative time
run_cmd "$PROGRAM -o 2400"              # Time beyond 24-hour format
run_cmd "$PROGRAM -o 2500"              # Invalid hour
run_cmd "$PROGRAM -o 999999999999999"   # Extremely large number

# Minutes specification tests
run_cmd "$PROGRAM -o 830"               # Has minutes (should fail as per your code)
run_cmd "$PROGRAM -o 815"               # Has minutes (should fail)
run_cmd "$PROGRAM -o 801"               # Has minutes (should fail)

# Test combinations with other options
echo "========== TESTING OPTION COMBINATIONS ============"
run_cmd "$PROGRAM -t 15 -o 0900"        # Valid combination
run_cmd "$PROGRAM -o 0900 -t 15"        # Valid combination in different order

# Add closing hour tests when implemented
echo "========== TESTING -c OPTION ============"
run_cmd "$PROGRAM -c 2200"              # Valid closing hour (10 PM)
run_cmd "$PROGRAM -o 0900 -c 1700"      # Valid opening and closing hours
run_cmd "$PROGRAM -c 0800 -o 0900"      # Invalid: closing before opening

# Redirect output to a file as well
echo "All test results have been displayed in the terminal."