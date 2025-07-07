#!/bin/bash

# QNX Space Launch System - Quick Run Script
# This script provides a convenient way to run the simulation from the project root

set -e  # Exit on any error

# Get the directory where this script is located (project root)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="$PROJECT_ROOT/bin/space_launch_sim"
GUI_DIR="$PROJECT_ROOT/gui"

echo "QNX Space Launch System - Quick Launch"
echo "====================================="

# Parse command line arguments first to handle --gui early
GUI_MODE="false"
GUI_ARGS=""
for arg in "$@"; do
    case $arg in
        -g|--gui)
            GUI_MODE="true"
            ;;
        --gui-basic)
            GUI_MODE="true"
            GUI_ARGS="$GUI_ARGS --basic"
            ;;
        --gui-dev)
            GUI_MODE="true"
            GUI_ARGS="$GUI_ARGS --dev"
            ;;
        --gui-test)
            GUI_MODE="true"
            GUI_ARGS="$GUI_ARGS --test"
            ;;
        --gui-test-coverage)
            GUI_MODE="true"
            GUI_ARGS="$GUI_ARGS --test-coverage"
            ;;
        --gui-*)
            GUI_MODE="true"
            # Handle any other GUI arguments by stripping gui- prefix
            GUI_ARGS="$GUI_ARGS --${arg#--gui-}"
            ;;
    esac
done

# If GUI mode requested, launch GUI immediately
if [ "$GUI_MODE" = "true" ]; then
    echo "Launching GUI mode..."
    echo ""
    if [ -f "$GUI_DIR/launch_gui.sh" ]; then
        exec "$GUI_DIR/launch_gui.sh" $GUI_ARGS
    else
        echo "Error: GUI launcher not found: $GUI_DIR/launch_gui.sh"
        exit 1
    fi
fi

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Simulation not built yet. Building now..."
    echo ""
    
    # Try to build the project
    if [ -f "$PROJECT_ROOT/scripts/build.sh" ]; then
        "$PROJECT_ROOT/scripts/build.sh" debug
    else
        echo "Error: Build script not found. Please build manually with 'make'"
        exit 1
    fi
    
    # Check again if build was successful
    if [ ! -f "$EXECUTABLE" ]; then
        echo "Error: Build failed. Executable not found: $EXECUTABLE"
        exit 1
    fi
    
    echo ""
    echo "Build complete! Starting simulation..."
    echo ""
fi

# Parse command line arguments
MISSION_TIME="auto"
DEBUG_MODE="false"
SHOW_HELP="false"

# Process arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            SHOW_HELP="true"
            shift
            ;;
        -g|--gui)
            # Already handled above
            shift
            ;;
        --gui-*)
            # Already handled above
            shift
            ;;
        -d|--debug)
            DEBUG_MODE="true"
            shift
            ;;
        -t|--time)
            MISSION_TIME="$2"
            shift 2
            ;;
        --now)
            MISSION_TIME="now"
            shift
            ;;
        --t-*)
            MISSION_TIME="$1"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            SHOW_HELP="true"
            shift
            ;;
    esac
done

# Show help if requested
if [ "$SHOW_HELP" = "true" ]; then
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help           Show this help message"
    echo "  -g, --gui            Launch Enhanced GUI mode instead of terminal"
    echo "  --gui-basic          Launch Basic GUI mode"
    echo "  --gui-dev            Launch GUI in development mode"
    echo "  --gui-test           Run GUI test suite"
    echo "  --gui-test-coverage  Run GUI tests with coverage"
    echo "  -d, --debug          Run in debug mode with GDB"
    echo "  -t, --time TIME      Set mission time (auto, now, t-600, etc.)"
    echo "  --now                Start at T-0 (immediate launch)"
    echo "  --t-TIME             Start at specific T-minus time (e.g., --t-600)"
    echo ""
    echo "Examples:"
    echo "  $0                       # Run with default settings (T-2 hours)"
    echo "  $0 --gui                 # Launch Enhanced GUI mode"
    echo "  $0 --gui-basic           # Launch Basic GUI mode"
    echo "  $0 --gui-dev             # Launch GUI in development mode"
    echo "  $0 --gui-test            # Run GUI test suite"
    echo "  $0 --gui-test-coverage   # Run GUI tests with coverage"
    echo "  $0 --now                 # Start at T-0 (immediate launch)"
    echo "  $0 -t t-600              # Start at T-10 minutes"
    echo "  $0 --debug               # Run in debug mode"
    echo "  $0 -d --now              # Debug mode with immediate launch"
    echo ""
    exit 0
fi

# Change to project directory
cd "$PROJECT_ROOT"

# Create logs directory if it doesn't exist
mkdir -p logs

# Show current configuration
echo "Configuration:"
echo "  Project root: $PROJECT_ROOT"
echo "  Mission time: $MISSION_TIME"
echo "  Debug mode: $DEBUG_MODE"
echo ""

# Set up environment
export SLS_CONFIG_FILE="$PROJECT_ROOT/config/system.conf"
export SLS_LOG_DIR="$PROJECT_ROOT/logs"

# Check for existing log files and offer to clear them
if [ -f "logs/sls_simulation.log" ] && [ -s "logs/sls_simulation.log" ]; then
    echo "Previous log files found."
    echo -n "Clear logs before starting? [y/N]: "
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        rm -f logs/*.log logs/*.csv
        echo "Logs cleared."
    fi
    echo ""
fi

# Start the simulation
echo "Starting QNX Space Launch System Simulation..."
echo "Press Ctrl+C to stop the simulation"
echo ""
echo "Monitor logs in another terminal with:"
echo "  tail -f logs/sls_simulation.log"
echo "  tail -f logs/telemetry.csv"
echo ""

# Set up signal handler for cleanup
cleanup() {
    echo ""
    echo "Simulation stopped by user."
    echo ""
    echo "Log files available in: $PROJECT_ROOT/logs/"
    echo "  - sls_simulation.log  (main system log)"
    echo "  - telemetry.csv       (telemetry data)"
    echo ""
    exit 0
}
trap cleanup INT TERM

# Run based on mode
if [ "$DEBUG_MODE" = "true" ]; then
    echo "Launching in debug mode with GDB..."
    echo "GDB commands:"
    echo "  (gdb) run"
    echo "  (gdb) continue"
    echo "  (gdb) quit"
    echo ""
    
    case "$MISSION_TIME" in
        "auto")
            gdb --args "$EXECUTABLE"
            ;;
        "now")
            gdb --args "$EXECUTABLE" --mission-time t-0
            ;;
        "t-"*)
            gdb --args "$EXECUTABLE" --mission-time "$MISSION_TIME"
            ;;
        *)
            gdb --args "$EXECUTABLE" --mission-time "$MISSION_TIME"
            ;;
    esac
else
    # Normal execution
    case "$MISSION_TIME" in
        "auto")
            echo "Mission time: Automatic (T-2 hours countdown)"
            "$EXECUTABLE"
            ;;
        "now")
            echo "Mission time: T-0 (immediate launch)"
            "$EXECUTABLE" --mission-time t-0
            ;;
        "t-"*)
            echo "Mission time: $MISSION_TIME"
            "$EXECUTABLE" --mission-time "$MISSION_TIME"
            ;;
        *)
            echo "Mission time: Custom ($MISSION_TIME)"
            "$EXECUTABLE" --mission-time "$MISSION_TIME"
            ;;
    esac
fi

# Call cleanup on normal exit
cleanup
