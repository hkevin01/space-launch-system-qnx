#!/usr/bin/env python3
"""Minimal PyQt6 test window"""

import sys

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import QApplication, QLabel, QMainWindow


def main():
    app = QApplication(sys.argv)
    
    window = QMainWindow()
    window.setWindowTitle('Test Window')
    window.resize(300, 200)
    window.move(200, 200)  # Position at 200,200
    
    label = QLabel('Test GUI Window - If you can see this, PyQt6 works!', window)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    window.setCentralWidget(label)
    
    window.show()
    window.raise_()
    window.activateWindow()
    
    print('Window created and shown')
    print(f'Window position: {window.x()}, {window.y()}')
    print(f'Window size: {window.width()}x{window.height()}')
    print(f'Window visible: {window.isVisible()}')
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
