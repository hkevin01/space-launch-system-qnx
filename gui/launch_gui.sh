#!/bin/bash

# QNX Space Launch System GUI Launcher
# This script sets up the environment and launches the GUI

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUI_DIR="$PROJECT_ROOT/gui"

echo "QNX Space Launch System - GUI Launcher"
echo "======================================"

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed or not in PATH"
    exit 1
fi

# Check if PyQt6 is installed
if ! python3 -c "import PyQt6" 2>/dev/null; then
    echo "PyQt6 not found. Installing requirements..."
    
    # Try to install PyQt6
    if command -v pip3 &> /dev/null; then
        pip3 install -r "$GUI_DIR/requirements.txt"
    elif command -v pip &> /dev/null; then
        pip install -r "$GUI_DIR/requirements.txt"
    else
        echo "Error: pip is not available. Please install PyQt6 manually:"
        echo "  pip install PyQt6"
        exit 1
    fi
fi

# Check if simulation binary exists
SIM_BINARY="$PROJECT_ROOT/bin/space_launch_sim"
if [ ! -f "$SIM_BINARY" ]; then
    echo "Simulation binary not found. Building project..."
    cd "$PROJECT_ROOT"
    make clean && make
    
    if [ ! -f "$SIM_BINARY" ]; then
        echo "Error: Failed to build simulation binary"
        exit 1
    fi
fi

echo "Starting GUI..."
cd "$GUI_DIR"
python3 launch_gui.py
