#!/usr/bin/env python3
"""Debug GUI launcher to test window visibility"""

import os
import sys

from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtWidgets import (
    QApplication,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class DebugGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Debug GUI - Space Launch System")
        self.resize(600, 400)
        
        # Set up central widget
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        
        # Add debug info
        layout.addWidget(QLabel("Debug GUI Window"))
        layout.addWidget(QLabel(f"Display: {os.environ.get('DISPLAY', 'Not set')}"))
        layout.addWidget(QLabel(f"Wayland: {os.environ.get('WAYLAND_DISPLAY', 'Not set')}"))
        layout.addWidget(QLabel(f"Session: {os.environ.get('XDG_SESSION_TYPE', 'Not set')}"))
        layout.addWidget(QLabel(f"QT Platform: {os.environ.get('QT_QPA_PLATFORM', 'Default')}"))
        
        # Add position button
        pos_btn = QPushButton("Print Position")
        pos_btn.clicked.connect(self.print_position)
        layout.addWidget(pos_btn)
        
        # Try multiple positioning strategies
        self.try_positioning()
        
        # Set up timer to keep trying to show window
        self.timer = QTimer()
        self.timer.timeout.connect(self.ensure_visible)
        self.timer.start(1000)  # Try every second
        
    def try_positioning(self):
        """Try different positioning strategies"""
        app = QApplication.instance()
        if app:
            screen = app.primaryScreen()
            if screen:
                rect = screen.availableGeometry()
                print(f"Screen available geometry: {rect}")
                
                # Try positioning in the middle of the available area
                x = rect.x() + 100
                y = rect.y() + 100
                self.move(x, y)
                print(f"Positioned at: ({x}, {y})")
            else:
                self.move(200, 200)
                print("No screen info, positioned at (200, 200)")
        
    def print_position(self):
        """Print current window position and state"""
        print(f"Window position: ({self.x()}, {self.y()})")
        print(f"Window size: {self.width()}x{self.height()}")
        print(f"Window visible: {self.isVisible()}")
        print(f"Window minimized: {self.isMinimized()}")
        print(f"Window active: {self.isActiveWindow()}")
        
    def ensure_visible(self):
        """Try to ensure window is visible"""
        if not self.isVisible():
            print("Window not visible, trying to show...")
            self.show()
        
        if not self.isActiveWindow():
            self.raise_()
            self.activateWindow()

def main():
    app = QApplication(sys.argv)
    
    print("Creating debug GUI...")
    window = DebugGUI()
    
    print("Showing window...")
    window.show()
    window.raise_()
    window.activateWindow()
    
    print("Window created. If you don't see it, check terminal output.")
    window.print_position()
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
