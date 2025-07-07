#!/bin/bash

# QNX Space Launch System GUI Launcher
# This script sets up the environment and launches the GUI

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUI_DIR="$PROJECT_ROOT/gui"

echo "QNX Space Launch System - GUI Launcher"
echo "======================================"

# Parse command line arguments
DEVELOPMENT_MODE="false"
VERBOSE="false"
TEST_MODE="false"
ENHANCED_GUI="true"  # Use enhanced GUI by default
BASIC_GUI="false"
COVERAGE_MODE="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--dev|--development)
            DEVELOPMENT_MODE="true"
            shift
            ;;
        -v|--verbose)
            VERBOSE="true"
            shift
            ;;
        -t|--test)
            TEST_MODE="true"
            shift
            ;;
        --test-coverage)
            TEST_MODE="true"
            COVERAGE_MODE="true"
            shift
            ;;
        --basic)
            ENHANCED_GUI="false"
            BASIC_GUI="true"
            shift
            ;;
        --enhanced)
            ENHANCED_GUI="true"
            BASIC_GUI="false"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --dev          Run in development mode"
            echo "  -v, --verbose      Enable verbose output"
            echo "  -t, --test         Run GUI tests instead of application"
            echo "  --test-coverage    Run tests with coverage report"
            echo "  --basic            Use basic GUI (default: enhanced)"
            echo "  --enhanced         Use enhanced GUI (default)"
            echo "  -h, --help         Show this help message"
            echo ""
            echo "GUI Modes:"
            echo "  Enhanced GUI       Modern interface with plots, monitoring, config"
            echo "  Basic GUI          Simple interface for basic operations"
            echo ""
            echo "Test Modes:"
            echo "  --test             Run test suite with pytest"
            echo "  --test-coverage    Run tests with coverage analysis"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed or not in PATH"
    echo "Please install Python 3.8 or later"
    exit 1
fi

# Check Python version
PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
if [[ "$VERBOSE" == "true" ]]; then
    echo "Python version: $PYTHON_VERSION"
fi

# Create virtual environment if in development mode
if [[ "$DEVELOPMENT_MODE" == "true" ]]; then
    VENV_DIR="$GUI_DIR/venv"
    if [[ ! -d "$VENV_DIR" ]]; then
        echo "Creating Python virtual environment..."
        python3 -m venv "$VENV_DIR"
    fi
    
    echo "Activating virtual environment..."
    source "$VENV_DIR/bin/activate"
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

# Install test dependencies if in test mode
if [[ "$TEST_MODE" == "true" ]]; then
    echo "Installing test dependencies..."
    if command -v pip3 &> /dev/null; then
        pip3 install pytest pytest-qt pytest-mock pytest-cov pytest-html
    elif command -v pip &> /dev/null; then
        pip install pytest pytest-qt pytest-mock pytest-cov pytest-html
    fi
fi

# Verify PyQt6 installation
if [[ "$VERBOSE" == "true" ]]; then
    echo "Checking PyQt6 installation..."
    python3 -c "import PyQt6; print(f'PyQt6 version: {PyQt6.QtCore.qVersion()}')"
    
    if [[ "$TEST_MODE" == "true" ]]; then
        echo "Checking test dependencies..."
        python3 -c "import pytest; print(f'pytest version: {pytest.__version__}')"
        python3 -c "import pytestqt; print('pytest-qt available')" 2>/dev/null || echo "pytest-qt not available"
    fi
fi

# Set up environment variables
export SLS_GUI_MODE="true"
export SLS_PROJECT_ROOT="$PROJECT_ROOT"
export SLS_CONFIG_FILE="$PROJECT_ROOT/config/system.conf"
export SLS_LOG_DIR="$PROJECT_ROOT/logs"

# Set up Qt platform - try Wayland first, then X11 fallback
if [[ "$XDG_SESSION_TYPE" == "wayland" ]] && command -v wayland-scanner &> /dev/null; then
    echo "Detected Wayland session, trying Wayland backend first..."
    export QT_QPA_PLATFORM=wayland
else
    echo "Using X11 backend..."
    export QT_QPA_PLATFORM=xcb
fi

# Also set some additional Qt environment variables for better compatibility
export QT_AUTO_SCREEN_SCALE_FACTOR=1
export QT_ENABLE_HIGHDPI_SCALING=1

if [[ "$DEVELOPMENT_MODE" == "true" ]]; then
    export SLS_DEVELOPMENT_MODE="true"
    export PYTHONPATH="$GUI_DIR:$PYTHONPATH"
fi

if [[ "$VERBOSE" == "true" ]]; then
    export SLS_VERBOSE="true"
fi

# Check if simulation binary exists (only if not in test mode)
if [[ "$TEST_MODE" != "true" ]]; then
    SIM_BINARY="$PROJECT_ROOT/bin/space_launch_sim"
    if [[ ! -f "$SIM_BINARY" ]]; then
        echo "Simulation binary not found. Building project..."
        cd "$PROJECT_ROOT"
        
        if [[ -f "scripts/build.sh" ]]; then
            ./scripts/build.sh
        else
            make clean && make
        fi
        
        if [[ ! -f "$SIM_BINARY" ]]; then
            echo "Error: Failed to build simulation binary"
            exit 1
        fi
        
        echo "Build complete!"
    fi
fi

# Create logs directory if it doesn't exist
mkdir -p "$PROJECT_ROOT/logs"

# Change to GUI directory
cd "$GUI_DIR"

# Run based on mode
if [[ "$TEST_MODE" == "true" ]]; then
    echo "Running GUI test suite..."
    echo ""
    
    # Create test results directory
    mkdir -p test_results
    
    if [[ "$COVERAGE_MODE" == "true" ]]; then
        echo "Running tests with coverage analysis..."
        python3 -m pytest test_gui_simple.py \
            --verbose \
            --cov=enhanced_launch_gui \
            --cov=launch_gui \
            --cov-report=html:test_results/coverage \
            --cov-report=term \
            --html=test_results/test_report.html \
            --self-contained-html
        
        echo ""
        echo "Test results available:"
        echo "  - Coverage report: test_results/coverage/index.html"
        echo "  - Test report: test_results/test_report.html"
    else
        echo "Running simplified test suite..."
        python3 -m pytest test_gui_simple.py \
            --verbose \
            --html=test_results/test_report.html \
            --self-contained-html
        
        echo ""
        echo "Test report available: test_results/test_report.html"
        echo "Run with --test-coverage for comprehensive tests"
    fi
    
elif [[ "$DEVELOPMENT_MODE" == "true" ]]; then
    echo "Starting GUI in development mode..."
    echo "Development features enabled:"
    echo "  - Virtual environment: $VENV_DIR"
    echo "  - Verbose logging enabled"
    echo "  - Debug mode enabled"
    
    if [[ "$ENHANCED_GUI" == "true" ]]; then
        echo "  - Using Enhanced GUI"
        echo ""
        python3 enhanced_launch_gui.py --development
    else
        echo "  - Using Basic GUI"
        echo ""
        python3 launch_gui.py --development
    fi
    
else
    if [[ "$ENHANCED_GUI" == "true" ]]; then
        echo "Starting Enhanced GUI..."
        echo "Features available:"
        echo "  - Real-time telemetry plotting"
        echo "  - System performance monitoring"
        echo "  - Mission parameter configuration"
        echo "  - Tabbed interface with advanced controls"
        echo ""
        python3 enhanced_launch_gui.py
    else
        echo "Starting Basic GUI..."
        python3 launch_gui.py
    fi
fi
