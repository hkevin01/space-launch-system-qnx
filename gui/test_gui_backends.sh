#!/bin/bash

echo "=== PyQt6 GUI Debug Test ==="
echo "Environment:"
echo "  DISPLAY: $DISPLAY"
echo "  WAYLAND_DISPLAY: $WAYLAND_DISPLAY"
echo "  XDG_SESSION_TYPE: $XDG_SESSION_TYPE"
echo ""

echo "Testing PyQt6 import..."
python3 -c "from PyQt6 import QtCore, QtWidgets; print(f'PyQt6 QtCore version: {QtCore.qVersion()}')" || {
    echo "ERROR: PyQt6 import failed!"
    exit 1
}

echo ""
echo "Testing with Wayland backend..."
QT_QPA_PLATFORM=wayland python3 debug_gui.py &
GUI_PID=$!
sleep 3

echo "GUI process started with PID: $GUI_PID"
if ps -p $GUI_PID > /dev/null; then
    echo "GUI process is running"
    kill $GUI_PID 2>/dev/null
else
    echo "GUI process exited"
fi

echo ""
echo "Testing with X11 backend..."
QT_QPA_PLATFORM=xcb python3 debug_gui.py &
GUI_PID=$!
sleep 3

echo "GUI process started with PID: $GUI_PID"
if ps -p $GUI_PID > /dev/null; then
    echo "GUI process is running"
    kill $GUI_PID 2>/dev/null
else
    echo "GUI process exited"
fi

echo ""
echo "Test complete. If no GUI windows appeared, there may be a display issue."
