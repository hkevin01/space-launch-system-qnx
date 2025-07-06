#!/bin/bash

# QNX Space Launch System - Run Simulation Script
# This script starts the space launch simulation with appropriate configuration

set -e  # Exit on any error

echo "========================================"
echo "QNX Space Launch System Simulator"
echo "========================================"

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXECUTABLE="$PROJECT_ROOT/bin/space_launch_sim"
CONFIG_FILE="$PROJECT_ROOT/config/system.conf"
LOG_DIR="$PROJECT_ROOT/logs"

# Command line arguments
MISSION_TIME="${1:-auto}"
CONFIG_OVERRIDE="${2:-}"
DEBUG_MODE="${3:-false}"

echo "Project root: $PROJECT_ROOT"
echo "Mission time: $MISSION_TIME"
echo "Debug mode: $DEBUG_MODE"

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Simulation executable not found: $EXECUTABLE"
    echo "Please build the project first:"
    echo "  ./scripts/build.sh"
    exit 1
fi

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Configuration file not found: $CONFIG_FILE"
    exit 1
fi

# Create logs directory if it doesn't exist
mkdir -p "$LOG_DIR"

# Set up environment
export SLS_CONFIG_FILE="$CONFIG_FILE"
export SLS_LOG_DIR="$LOG_DIR"

# Apply config override if specified
if [ -n "$CONFIG_OVERRIDE" ]; then
    export SLS_CONFIG_OVERRIDE="$CONFIG_OVERRIDE"
    echo "Config override: $CONFIG_OVERRIDE"
fi

# QNX-specific setup
if [ -n "$QNX_TARGET" ]; then
    echo "QNX environment detected"
    export LD_LIBRARY_PATH="$QNX_TARGET/usr/lib:$LD_LIBRARY_PATH"
fi

# Display system info
echo ""
echo "System Information:"
echo "  CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "  Memory: $(free -h | grep Mem | awk '{print $2}')"
echo "  OS: $(uname -s -r)"

# Pre-flight checks
echo ""
echo "Pre-flight checks:"

# Check available memory
AVAILABLE_MEM=$(free -m | grep Available | awk '{print $7}')
if [ -n "$AVAILABLE_MEM" ] && [ "$AVAILABLE_MEM" -lt 512 ]; then
    echo "  Warning: Low available memory ($AVAILABLE_MEM MB)"
fi

# Check disk space
DISK_SPACE=$(df -h "$PROJECT_ROOT" | tail -1 | awk '{print $4}')
echo "  Disk space available: $DISK_SPACE"

# Check for conflicting processes
CONFLICTING_PROCS=$(pgrep -f "space_launch" || true)
if [ -n "$CONFLICTING_PROCS" ]; then
    echo "  Warning: Other simulation processes running (PIDs: $CONFLICTING_PROCS)"
fi

echo "  Configuration: $CONFIG_FILE"
echo "  Log directory: $LOG_DIR"
echo "  Ready for launch!"

# Start simulation
echo ""
echo "Starting Space Launch System Simulation..."
echo "Press Ctrl+C to stop the simulation"
echo ""

# Handle mission time parameter
MISSION_ARGS=""
case "$MISSION_TIME" in
    "auto")
        echo "Mission time: Automatic (T-2 hours)"
        ;;
    "t-*")
        echo "Mission time: $MISSION_TIME"
        MISSION_ARGS="--mission-time $MISSION_TIME"
        ;;
    "now")
        echo "Mission time: T-0 (immediate launch)"
        MISSION_ARGS="--mission-time t-0"
        ;;
    *)
        echo "Mission time: Custom ($MISSION_TIME)"
        MISSION_ARGS="--mission-time $MISSION_TIME"
        ;;
esac

# Run simulation with appropriate options
if [ "$DEBUG_MODE" = "true" ]; then
    echo "Running in debug mode..."
    gdb --args "$EXECUTABLE" $MISSION_ARGS
else
    # Set up signal handlers for graceful shutdown
    trap 'echo ""; echo "Simulation terminated by user"; exit 0' INT TERM
    
    # Run the simulation
    "$EXECUTABLE" $MISSION_ARGS
fi

echo ""
echo "Simulation ended."
