#!/usr/bin/env python3
"""
Space Launch System GUI
A modern PyQt6 interface for monitoring and controlling the QNX space launch simulation.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import psutil
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from PyQt6.QtCore import (
    QPropertyAnimation,
    QRect,
    QSettings,
    Qt,
    QThread,
    QTimer,
    pyqtSignal,
)
from PyQt6.QtGui import QAction, QColor, QFont, QIcon, QPalette, QPixmap
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QDoubleSpinBox,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMenuBar,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSlider,
    QSpinBox,
    QSplitter,
    QStatusBar,
    QTabWidget,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)


class SimulationMonitor(QThread):
    """Thread for monitoring the simulation process and parsing logs."""
    
    # Signals for updating GUI
    telemetry_update = pyqtSignal(dict)
    status_update = pyqtSignal(str, str)  # component, message
    phase_update = pyqtSignal(str)
    log_update = pyqtSignal(str)
    simulation_stopped = pyqtSignal()
    
    def __init__(self, project_root):
        super().__init__()
        self.project_root = Path(project_root)
        self.process = None
        self.running = False
        self.log_file_path = None
        
    def start_simulation(self):
        """Start the simulation process."""
        try:
            # Change to project directory and start simulation
            os.chdir(self.project_root)
            
            # Start the simulation process
            self.process = subprocess.Popen(
                ['./bin/space_launch_sim'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            self.running = True
            self.start()  # Start the monitoring thread
            
        except Exception as e:
            self.status_update.emit("ERROR", f"Failed to start simulation: {str(e)}")
            
    def stop_simulation(self):
        """Stop the simulation process."""
        self.running = False
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
        
    def run(self):
        """Main monitoring loop."""
        if not self.process:
            return
            
        # Find the latest log file
        log_dir = self.project_root / "logs"
        if log_dir.exists():
            log_files = list(log_dir.glob("space_launch_*.log"))
            if log_files:
                self.log_file_path = max(log_files, key=lambda p: p.stat().st_mtime)
        
        # Monitor process output and log files
        last_log_pos = 0
        
        while self.running and self.process and self.process.poll() is None:
            # Read from log file if available
            if self.log_file_path and self.log_file_path.exists():
                try:
                    with open(self.log_file_path, 'r') as f:
                        f.seek(last_log_pos)
                        new_lines = f.readlines()
                        last_log_pos = f.tell()
                        
                        for line in new_lines:
                            self.parse_log_line(line.strip())
                            
                except Exception as e:
                    pass  # Continue monitoring even if log reading fails
            
            time.sleep(0.1)  # 100ms polling interval
        
        if not self.running or (self.process and self.process.poll() is not None):
            self.simulation_stopped.emit()
    
    def parse_log_line(self, line):
        """Parse a log line and extract relevant information."""
        if not line:
            return
            
        self.log_update.emit(line)
        
        # Parse telemetry data
        if "Altitude" in line and "FCC" in line:
            altitude_match = re.search(r'Altitude.*?(\-?\d+\.?\d*)\s*m', line)
            if altitude_match:
                altitude = float(altitude_match.group(1))
                self.telemetry_update.emit({
                    'altitude': altitude,
                    'timestamp': datetime.now()
                })
        
        # Parse status messages
        if "] INFO " in line:
            parts = line.split("] INFO ")
            if len(parts) >= 2:
                component_msg = parts[1].split(":", 1)
                if len(component_msg) >= 2:
                    component = component_msg[0].strip()
                    message = component_msg[1].strip()
                    self.status_update.emit(component, message)
        
        # Parse phase changes
        if "Mission phase changed" in line:
            phase_match = re.search(r'phase changed to:\s*(\d+)', line)
            if phase_match:
                phase_num = int(phase_match.group(1))
                phase_names = {
                    0: "PRE-LAUNCH",
                    1: "IGNITION", 
                    2: "LIFTOFF",
                    3: "ASCENT",
                    4: "STAGE SEPARATION",
                    5: "ORBIT INSERTION",
                    6: "MISSION COMPLETE",
                    7: "ABORT"
                }
                phase_name = phase_names.get(phase_num, f"PHASE {phase_num}")
                self.phase_update.emit(phase_name)

class TelemetryWidget(QGroupBox):
    """Widget for displaying real-time telemetry data."""
    
    def __init__(self):
        super().__init__("Vehicle Telemetry")
        self.init_ui()
        
    def init_ui(self):
        layout = QGridLayout()
        
        # Altitude display
        layout.addWidget(QLabel("Altitude:"), 0, 0)
        self.altitude_label = QLabel("0.0 m")
        self.altitude_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(self.altitude_label, 0, 1)
        
        # Velocity display
        layout.addWidget(QLabel("Velocity:"), 1, 0)
        self.velocity_label = QLabel("0.0 m/s")
        self.velocity_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(self.velocity_label, 1, 1)
        
        # Mission time
        layout.addWidget(QLabel("Mission Time:"), 2, 0)
        self.mission_time_label = QLabel("T-00:00:00")
        self.mission_time_label.setStyleSheet("font-weight: bold; font-size: 14px; color: #00ff00;")
        layout.addWidget(self.mission_time_label, 2, 1)
        
        # Current phase
        layout.addWidget(QLabel("Phase:"), 3, 0)
        self.phase_label = QLabel("PRE-LAUNCH")
        self.phase_label.setStyleSheet("font-weight: bold; font-size: 14px; color: #ffaa00;")
        layout.addWidget(self.phase_label, 3, 1)
        
        self.setLayout(layout)
        
    def update_telemetry(self, data):
        """Update telemetry displays."""
        if 'altitude' in data:
            altitude = data['altitude']
            self.altitude_label.setText(f"{altitude:.1f} m")
            
            # Color code altitude
            if altitude < 0:
                color = "#ff4444"  # Red for below ground
            elif altitude < 1000:
                color = "#ffaa00"  # Orange for low altitude
            else:
                color = "#00ff00"  # Green for normal flight
                
            self.altitude_label.setStyleSheet(f"font-weight: bold; font-size: 14px; color: {color};")
    
    def update_phase(self, phase):
        """Update mission phase display."""
        self.phase_label.setText(phase)
        
        # Color code phases
        phase_colors = {
            "PRE-LAUNCH": "#ffaa00",
            "IGNITION": "#ff6600", 
            "LIFTOFF": "#ff0000",
            "ASCENT": "#00ff00",
            "STAGE SEPARATION": "#00aaff",
            "ORBIT INSERTION": "#aa00ff",
            "MISSION COMPLETE": "#00ff00",
            "ABORT": "#ff0000"
        }
        color = phase_colors.get(phase, "#ffffff")
        self.phase_label.setStyleSheet(f"font-weight: bold; font-size: 14px; color: {color};")

class StatusWidget(QGroupBox):
    """Widget for displaying system status messages."""
    
    def __init__(self):
        super().__init__("System Status")
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        self.status_text = QTextEdit()
        self.status_text.setMaximumHeight(200)
        self.status_text.setReadOnly(True)
        self.status_text.setStyleSheet("""
            QTextEdit {
                background-color: #1e1e1e;
                color: #ffffff;
                font-family: 'Courier New', monospace;
                font-size: 10px;
            }
        """)
        
        layout.addWidget(self.status_text)
        self.setLayout(layout)
        
    def add_status(self, component, message):
        """Add a status message."""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.status_text.append(f"[{timestamp}] {component}: {message}")
        
        # Auto-scroll to bottom
        scrollbar = self.status_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

class LogWidget(QGroupBox):
    """Widget for displaying full simulation logs."""
    
    def __init__(self):
        super().__init__("Simulation Log")
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setStyleSheet("""
            QTextEdit {
                background-color: #0d1117;
                color: #c9d1d9;
                font-family: 'Courier New', monospace;
                font-size: 9px;
            }
        """)
        
        layout.addWidget(self.log_text)
        self.setLayout(layout)
        
    def add_log(self, line):
        """Add a log line."""
        self.log_text.append(line)
        
        # Keep only last 1000 lines to prevent memory issues
        if self.log_text.document().lineCount() > 1000:
            cursor = self.log_text.textCursor()
            cursor.movePosition(cursor.MoveOperation.Start)
            cursor.movePosition(cursor.MoveOperation.Down, cursor.MoveMode.KeepAnchor, 100)
            cursor.removeSelectedText()
        
        # Auto-scroll to bottom
        scrollbar = self.log_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

class LaunchControlGUI(QMainWindow):
    """Main GUI application for the Space Launch System."""
    
    def __init__(self):
        super().__init__()
        
        # Find project root
        self.project_root = Path(__file__).parent.parent
        
        self.simulation_monitor = None
        self.init_ui()
        
    def init_ui(self):
        """Initialize the user interface."""
        self.setWindowTitle("QNX Space Launch System - Mission Control")
        self.setGeometry(100, 100, 1200, 800)
        
        # Set dark theme
        self.setStyleSheet("""
            QMainWindow {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QGroupBox {
                font-weight: bold;
                border: 2px solid #555555;
                border-radius: 5px;
                margin-top: 1ex;
                padding-top: 5px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
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
            QPushButton:pressed {
                background-color: #353535;
            }
            QLabel {
                color: #ffffff;
            }
        """)
        
        # Create central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Main layout
        main_layout = QVBoxLayout(central_widget)
        
        # Header with title and mission status
        header_layout = QHBoxLayout()
        
        title_label = QLabel("🚀 QNX SPACE LAUNCH SYSTEM")
        title_label.setStyleSheet("font-size: 20px; font-weight: bold; color: #00aaff;")
        header_layout.addWidget(title_label)
        
        header_layout.addStretch()
        
        self.mission_status_label = QLabel("STANDBY")
        self.mission_status_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ffaa00;")
        header_layout.addWidget(self.mission_status_label)
        
        main_layout.addLayout(header_layout)
        
        # Control buttons
        button_layout = QHBoxLayout()
        
        self.start_button = QPushButton("🚀 START MISSION")
        self.start_button.setStyleSheet("""
            QPushButton {
                background-color: #2d5a2d;
                font-weight: bold;
                font-size: 12px;
            }
            QPushButton:hover {
                background-color: #3d6a3d;
            }
        """)
        self.start_button.clicked.connect(self.start_simulation)
        button_layout.addWidget(self.start_button)
        
        self.stop_button = QPushButton("🛑 STOP MISSION")
        self.stop_button.setStyleSheet("""
            QPushButton {
                background-color: #5a2d2d;
                font-weight: bold;
                font-size: 12px;
            }
            QPushButton:hover {
                background-color: #6a3d3d;
            }
        """)
        self.stop_button.clicked.connect(self.stop_simulation)
        self.stop_button.setEnabled(False)
        button_layout.addWidget(self.stop_button)
        
        button_layout.addStretch()
        
        main_layout.addLayout(button_layout)
        
        # Create splitter for resizable panels
        splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # Left panel - Telemetry and Status
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        
        self.telemetry_widget = TelemetryWidget()
        left_layout.addWidget(self.telemetry_widget)
        
        self.status_widget = StatusWidget()
        left_layout.addWidget(self.status_widget)
        
        left_layout.addStretch()
        
        # Right panel - Logs
        self.log_widget = LogWidget()
        
        splitter.addWidget(left_panel)
        splitter.addWidget(self.log_widget)
        splitter.setSizes([400, 800])
        
        main_layout.addWidget(splitter)
        
        # Status bar
        self.status_bar = QStatusBar()
        self.status_bar.showMessage("Ready to launch")
        self.setStatusBar(self.status_bar)
        
    def start_simulation(self):
        """Start the simulation."""
        if self.simulation_monitor and self.simulation_monitor.running:
            return
            
        # Check if simulation binary exists
        sim_binary = self.project_root / "bin" / "space_launch_sim"
        if not sim_binary.exists():
            QMessageBox.warning(self, "Error", 
                              "Simulation binary not found. Please build the project first.")
            return
        
        self.mission_status_label.setText("LAUNCHING...")
        self.mission_status_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ff6600;")
        
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        
        # Create and start monitor thread
        self.simulation_monitor = SimulationMonitor(self.project_root)
        self.simulation_monitor.telemetry_update.connect(self.telemetry_widget.update_telemetry)
        self.simulation_monitor.status_update.connect(self.status_widget.add_status)
        self.simulation_monitor.phase_update.connect(self.telemetry_widget.update_phase)
        self.simulation_monitor.log_update.connect(self.log_widget.add_log)
        self.simulation_monitor.simulation_stopped.connect(self.on_simulation_stopped)
        
        self.simulation_monitor.start_simulation()
        
        self.status_bar.showMessage("Simulation running...")
        
    def stop_simulation(self):
        """Stop the simulation."""
        if self.simulation_monitor:
            self.simulation_monitor.stop_simulation()
            
    def on_simulation_stopped(self):
        """Handle simulation stop."""
        self.mission_status_label.setText("STANDBY")
        self.mission_status_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ffaa00;")
        
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        
        self.status_bar.showMessage("Simulation stopped")
        
    def closeEvent(self, event):
        """Handle application close."""
        if self.simulation_monitor and self.simulation_monitor.running:
            reply = QMessageBox.question(self, 'Close Application',
                                       'Simulation is running. Stop and exit?',
                                       QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                                       QMessageBox.StandardButton.No)
            
            if reply == QMessageBox.StandardButton.Yes:
                self.stop_simulation()
                event.accept()
            else:
                event.ignore()
        else:
            event.accept()

def main():
    """Main entry point."""
    app = QApplication(sys.argv)
    
    # Set application properties
    app.setApplicationName("QNX Space Launch System")
    app.setApplicationVersion("1.0")
    app.setOrganizationName("QNX Aerospace")
    
    # Create and show main window
    window = LaunchControlGUI()
    window.show()
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
