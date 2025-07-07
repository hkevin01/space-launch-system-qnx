#!/usr/bin/env python3
"""Minimal Enhanced GUI test"""

import sys

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QApplication,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class MinimalEnhancedGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle("QNX Space Launch System - Minimal Test")
        self.resize(800, 600)
        self.move(100, 100)
        
        # Set dark theme
        self.setStyleSheet("""
            QMainWindow {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QPushButton {
                background-color: #404040;
                border: 1px solid #666666;
                border-radius: 4px;
                padding: 5px;
                min-width: 80px;
            }
            QPushButton:hover {
                background-color: #505050;
            }
            QLabel {
                color: #ffffff;
            }
        """)
        
        # Create central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        layout = QVBoxLayout(central_widget)
        
        # Add some content
        title_label = QLabel("🚀 QNX SPACE LAUNCH SYSTEM - MINIMAL TEST")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #00aaff;")
        title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title_label)
        
        status_label = QLabel("If you can see this, the minimal enhanced GUI works!")
        status_label.setStyleSheet("font-size: 14px; color: #00ff00;")
        status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(status_label)
        
        test_button = QPushButton("Test Button")
        test_button.clicked.connect(lambda: print("Button clicked!"))
        layout.addWidget(test_button)
        
        layout.addStretch()

def main():
    app = QApplication(sys.argv)
    
    print("Creating minimal enhanced GUI...")
    window = MinimalEnhancedGUI()
    
    window.show()
    window.raise_()
    window.activateWindow()
    
    print("Minimal GUI window should be visible")
    print(f"Window position: ({window.x()}, {window.y()})")
    print(f"Window size: {window.width()}x{window.height()}")
    print(f"Window visible: {window.isVisible()}")
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
