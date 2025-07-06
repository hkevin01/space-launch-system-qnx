#!/bin/bash

# QNX Space Launch System - Build Script
# This script builds the entire simulation system

set -e  # Exit on any error

echo "========================================"
echo "QNX Space Launch System Build Script"
echo "========================================"

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${1:-debug}"
CLEAN_BUILD="${2:-false}"

echo "Project root: $PROJECT_ROOT"
echo "Build type: $BUILD_TYPE"
echo "Clean build: $CLEAN_BUILD"

# Check for QNX environment
if command -v qcc > /dev/null 2>&1; then
    echo "QNX environment detected"
    echo "QNX Host: $QNX_HOST"
    echo "QNX Target: $QNX_TARGET"
    QNX_BUILD=true
else
    echo "QNX environment not found - building in Linux simulation mode"
    echo "Note: QNX-specific features will be mocked for development"
    QNX_BUILD=false
fi

# Change to project directory
cd "$PROJECT_ROOT"

# Clean build if requested
if [ "$CLEAN_BUILD" = "true" ]; then
    echo "Performing clean build..."
    make clean
fi

# Build based on type
case "$BUILD_TYPE" in
    "debug")
        echo "Building debug version..."
        make debug
        ;;
    "release")
        echo "Building release version..."
        make release
        ;;
    "all")
        echo "Building all configurations..."
        make clean
        make debug
        make clean
        make release
        ;;
    *)
        echo "Building default configuration..."
        make all
        ;;
esac

# Check if build was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Build completed successfully!"
    echo "========================================"
    echo "Executable: bin/space_launch_sim"
    echo ""
    echo "To run the simulation:"
    echo "  ./scripts/run_simulation.sh"
    echo ""
    echo "To run tests:"
    echo "  make test"
else
    echo ""
    echo "========================================"
    echo "Build failed!"
    echo "========================================"
    exit 1
fi
