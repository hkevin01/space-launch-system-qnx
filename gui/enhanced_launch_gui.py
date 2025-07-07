#!/usr/bin/env python3
"""
Enhanced Space Launch System GUI
A modern PyQt6 interface for monitoring and controlling the QNX space launch simulation
with advanced features including telemetry plotting, system monitoring, and testing capabilities.
"""

# Configure matplotlib backend before any other imports
import matplotlib

matplotlib.use('QtAgg')

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

import numpy as np
import psutil
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
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


class SystemMetrics:
    """Collects and tracks system performance metrics."""
    
    def __init__(self):
        self.cpu_usage = []
        self.memory_usage = []
        self.timestamps = []
        self.max_samples = 100
        
    def update(self):
        """Update system metrics."""
        current_time = time.time()
        cpu_percent = psutil.cpu_percent()
        memory_percent = psutil.virtual_memory().percent
        
        self.timestamps.append(current_time)
        self.cpu_usage.append(cpu_percent)
        self.memory_usage.append(memory_percent)
        
        # Keep only the last max_samples
        if len(self.timestamps) > self.max_samples:
            self.timestamps.pop(0)
            self.cpu_usage.pop(0)
            self.memory_usage.pop(0)
    
    def get_latest_metrics(self):
        """Get the latest system metrics."""
        if not self.timestamps:
            return {"cpu": 0, "memory": 0, "timestamp": time.time()}
        
        return {
            "cpu": self.cpu_usage[-1],
            "memory": self.memory_usage[-1],
            "timestamp": self.timestamps[-1]
        }


class TelemetryPlotter(QWidget):
    """Widget for plotting real-time telemetry data."""
    
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.telemetry_data = {
            'altitude': [],
            'velocity': [],
            'timestamps': []
        }
        self.max_samples = 500
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        # Create matplotlib figure
        self.figure = Figure(figsize=(12, 8))
        self.canvas = FigureCanvas(self.figure)
        layout.addWidget(self.canvas)
        
        # Control panel
        controls_layout = QHBoxLayout()
        
        self.auto_scale_checkbox = QCheckBox("Auto Scale")
        self.auto_scale_checkbox.setChecked(True)
        controls_layout.addWidget(self.auto_scale_checkbox)
        
        controls_layout.addWidget(QLabel("Time Window (s):"))
        self.time_window_spin = QSpinBox()
        self.time_window_spin.setRange(10, 300)
        self.time_window_spin.setValue(60)
        controls_layout.addWidget(self.time_window_spin)
        
        self.clear_button = QPushButton("Clear Data")
        self.clear_button.clicked.connect(self.clear_data)
        controls_layout.addWidget(self.clear_button)
        
        controls_layout.addStretch()
        layout.addLayout(controls_layout)
        
        self.setLayout(layout)
        self.setup_plots()
        
    def setup_plots(self):
        """Set up the subplot layout."""
        self.figure.clear()
        
        # Altitude plot
        self.altitude_ax = self.figure.add_subplot(3, 1, 1)
        self.altitude_ax.set_title('Vehicle Altitude')
        self.altitude_ax.set_ylabel('Altitude (m)')
        self.altitude_ax.grid(True, alpha=0.3)
        
        # Velocity plot
        self.velocity_ax = self.figure.add_subplot(3, 1, 2)
        self.velocity_ax.set_title('Vehicle Velocity')
        self.velocity_ax.set_ylabel('Velocity (m/s)')
        self.velocity_ax.grid(True, alpha=0.3)
        
        # Phase indicator
        self.phase_ax = self.figure.add_subplot(3, 1, 3)
        self.phase_ax.set_title('Mission Phase')
        self.phase_ax.set_ylabel('Phase')
        self.phase_ax.set_xlabel('Time (s)')
        self.phase_ax.grid(True, alpha=0.3)
        
        self.figure.tight_layout()
        self.canvas.draw()
        
    def update_telemetry(self, data):
        """Update telemetry plots with new data."""
        try:
            if 'altitude' in data and data['altitude'] is not None:
                # Validate that altitude is numeric
                altitude = float(data['altitude'])
                velocity = float(data.get('velocity', 0))
                
                current_time = time.time()
                self.telemetry_data['timestamps'].append(current_time)
                self.telemetry_data['altitude'].append(altitude)
                self.telemetry_data['velocity'].append(velocity)
                
                # Keep only recent data
                if len(self.telemetry_data['timestamps']) > self.max_samples:
                    self.telemetry_data['timestamps'].pop(0)
                    self.telemetry_data['altitude'].pop(0)
                    self.telemetry_data['velocity'].pop(0)
                
                self.refresh_plots()
        except (ValueError, TypeError):
            # Silently ignore invalid data
            pass
    
    def refresh_plots(self):
        """Refresh the plots with current data."""
        if not self.telemetry_data['timestamps']:
            return
            
        # Convert to relative time
        base_time = self.telemetry_data['timestamps'][0]
        times = [(t - base_time) for t in self.telemetry_data['timestamps']]
        
        # Update altitude plot
        self.altitude_ax.clear()
        self.altitude_ax.plot(times, self.telemetry_data['altitude'], 'b-', linewidth=2)
        self.altitude_ax.set_title('Vehicle Altitude')
        self.altitude_ax.set_ylabel('Altitude (m)')
        self.altitude_ax.grid(True, alpha=0.3)
        
        # Update velocity plot
        self.velocity_ax.clear()
        self.velocity_ax.plot(times, self.telemetry_data['velocity'], 'r-', linewidth=2)
        self.velocity_ax.set_title('Vehicle Velocity')
        self.velocity_ax.set_ylabel('Velocity (m/s)')
        self.velocity_ax.grid(True, alpha=0.3)
        
        # Auto-scale if enabled
        if self.auto_scale_checkbox.isChecked():
            time_window = self.time_window_spin.value()
            if times and times[-1] > time_window:
                self.altitude_ax.set_xlim(times[-1] - time_window, times[-1])
                self.velocity_ax.set_xlim(times[-1] - time_window, times[-1])
        
        self.figure.tight_layout()
        self.canvas.draw()
    
    def clear_data(self):
        """Clear all telemetry data."""
        self.telemetry_data = {
            'altitude': [],
            'velocity': [],
            'timestamps': []
        }
        self.setup_plots()


class SystemMonitorWidget(QGroupBox):
    """Widget for monitoring system performance."""
    
    def __init__(self):
        super().__init__("System Performance")
        self.metrics = SystemMetrics()
        self.init_ui()
        
        # Timer for updating metrics
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_display)
        self.timer.start(1000)  # Update every second
        
    def init_ui(self):
        layout = QGridLayout()
        
        # CPU usage
        layout.addWidget(QLabel("CPU Usage:"), 0, 0)
        self.cpu_label = QLabel("0%")
        self.cpu_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.cpu_label, 0, 1)
        
        self.cpu_progress = QProgressBar()
        self.cpu_progress.setMaximum(100)
        layout.addWidget(self.cpu_progress, 0, 2)
        
        # Memory usage
        layout.addWidget(QLabel("Memory Usage:"), 1, 0)
        self.memory_label = QLabel("0%")
        self.memory_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.memory_label, 1, 1)
        
        self.memory_progress = QProgressBar()
        self.memory_progress.setMaximum(100)
        layout.addWidget(self.memory_progress, 1, 2)
        
        # Process count
        layout.addWidget(QLabel("Active Processes:"), 2, 0)
        self.process_label = QLabel("0")
        self.process_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.process_label, 2, 1)
        
        # Simulation status
        layout.addWidget(QLabel("Simulation PID:"), 3, 0)
        self.sim_pid_label = QLabel("Not Running")
        self.sim_pid_label.setStyleSheet("font-weight: bold; color: #ff6666;")
        layout.addWidget(self.sim_pid_label, 3, 1)
        
        self.setLayout(layout)
    
    def update_display(self):
        """Update the performance display."""
        self.metrics.update()
        latest = self.metrics.get_latest_metrics()
        
        # Update CPU
        cpu_percent = int(latest['cpu'])
        self.cpu_label.setText(f"{cpu_percent}%")
        self.cpu_progress.setValue(cpu_percent)
        
        # Color code CPU usage
        if cpu_percent > 80:
            self.cpu_progress.setStyleSheet("QProgressBar::chunk { background-color: #ff4444; }")
        elif cpu_percent > 60:
            self.cpu_progress.setStyleSheet("QProgressBar::chunk { background-color: #ffaa44; }")
        else:
            self.cpu_progress.setStyleSheet("QProgressBar::chunk { background-color: #44ff44; }")
        
        # Update Memory
        memory_percent = int(latest['memory'])
        self.memory_label.setText(f"{memory_percent}%")
        self.memory_progress.setValue(memory_percent)
        
        # Color code memory usage
        if memory_percent > 80:
            self.memory_progress.setStyleSheet("QProgressBar::chunk { background-color: #ff4444; }")
        elif memory_percent > 60:
            self.memory_progress.setStyleSheet("QProgressBar::chunk { background-color: #ffaa44; }")
        else:
            self.memory_progress.setStyleSheet("QProgressBar::chunk { background-color: #44ff44; }")
        
        # Update process count
        process_count = len(psutil.pids())
        self.process_label.setText(str(process_count))
        
        # Check for simulation process
        sim_running = False
        sim_pid = "Not Running"
        for proc in psutil.process_iter(['pid', 'name']):
            try:
                if 'space_launch_sim' in proc.info['name']:
                    sim_running = True
                    sim_pid = str(proc.info['pid'])
                    break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        
        if sim_running:
            self.sim_pid_label.setText(sim_pid)
            self.sim_pid_label.setStyleSheet("font-weight: bold; color: #44ff44;")
        else:
            self.sim_pid_label.setText("Not Running")
            self.sim_pid_label.setStyleSheet("font-weight: bold; color: #ff6666;")


class MissionParametersDialog(QDialog):
    """Dialog for configuring mission parameters."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Mission Parameters")
        self.setModal(True)
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        # Mission parameters group
        params_group = QGroupBox("Launch Parameters")
        params_layout = QGridLayout()
        
        # Mission time
        params_layout.addWidget(QLabel("Mission Time:"), 0, 0)
        self.mission_time_combo = QComboBox()
        self.mission_time_combo.addItems([
            "Auto (T-2 hours)",
            "T-0 (Immediate)",
            "T-600 (10 minutes)",
            "T-300 (5 minutes)",
            "T-60 (1 minute)"
        ])
        params_layout.addWidget(self.mission_time_combo, 0, 1)
        
        # Vehicle mass
        params_layout.addWidget(QLabel("Vehicle Mass (kg):"), 1, 0)
        self.vehicle_mass_spin = QDoubleSpinBox()
        self.vehicle_mass_spin.setRange(100000, 2000000)
        self.vehicle_mass_spin.setValue(500000)
        self.vehicle_mass_spin.setSuffix(" kg")
        params_layout.addWidget(self.vehicle_mass_spin, 1, 1)
        
        # Target altitude
        params_layout.addWidget(QLabel("Target Altitude (m):"), 2, 0)
        self.target_altitude_spin = QDoubleSpinBox()
        self.target_altitude_spin.setRange(100000, 1000000)
        self.target_altitude_spin.setValue(400000)
        self.target_altitude_spin.setSuffix(" m")
        params_layout.addWidget(self.target_altitude_spin, 2, 1)
        
        # Simulation rate
        params_layout.addWidget(QLabel("Simulation Rate (Hz):"), 3, 0)
        self.sim_rate_spin = QSpinBox()
        self.sim_rate_spin.setRange(10, 1000)
        self.sim_rate_spin.setValue(100)
        self.sim_rate_spin.setSuffix(" Hz")
        params_layout.addWidget(self.sim_rate_spin, 3, 1)
        
        params_group.setLayout(params_layout)
        layout.addWidget(params_group)
        
        # Options group
        options_group = QGroupBox("Options")
        options_layout = QVBoxLayout()
        
        self.enable_faults_checkbox = QCheckBox("Enable Fault Injection")
        options_layout.addWidget(self.enable_faults_checkbox)
        
        self.verbose_logging_checkbox = QCheckBox("Verbose Logging")
        options_layout.addWidget(self.verbose_logging_checkbox)
        
        self.record_telemetry_checkbox = QCheckBox("Record Telemetry to File")
        self.record_telemetry_checkbox.setChecked(True)
        options_layout.addWidget(self.record_telemetry_checkbox)
        
        options_group.setLayout(options_layout)
        layout.addWidget(options_group)
        
        # Buttons
        button_layout = QHBoxLayout()
        
        self.ok_button = QPushButton("Launch Mission")
        self.ok_button.clicked.connect(self.accept)
        button_layout.addWidget(self.ok_button)
        
        self.cancel_button = QPushButton("Cancel")
        self.cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(self.cancel_button)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
    
    def get_parameters(self):
        """Get the configured parameters."""
        mission_time_map = {
            0: "auto",
            1: "t-0",
            2: "t-600",
            3: "t-300", 
            4: "t-60"
        }
        
        return {
            'mission_time': mission_time_map[self.mission_time_combo.currentIndex()],
            'vehicle_mass': self.vehicle_mass_spin.value(),
            'target_altitude': self.target_altitude_spin.value(),
            'simulation_rate': self.sim_rate_spin.value(),
            'enable_faults': self.enable_faults_checkbox.isChecked(),
            'verbose_logging': self.verbose_logging_checkbox.isChecked(),
            'record_telemetry': self.record_telemetry_checkbox.isChecked()
        }


# Import the original classes (slightly modified)
class SimulationMonitor(QThread):
    """Thread for monitoring the simulation process and parsing logs."""
    
    # Signals for updating GUI
    telemetry_update = pyqtSignal(dict)
    status_update = pyqtSignal(str, str)  # component, message
    phase_update = pyqtSignal(str)
    log_update = pyqtSignal(str)
    simulation_stopped = pyqtSignal()
    
    def __init__(self, project_root, parameters=None):
        super().__init__()
        self.project_root = Path(project_root)
        self.parameters = parameters or {}
        self.process = None
        self.running = False
        self.log_file_path = None
        
    def start_simulation(self):
        """Start the simulation process with configured parameters."""
        try:
            # Change to project directory
            os.chdir(self.project_root)
            
            # Build command with parameters
            cmd = ['./bin/space_launch_sim']
            
            if 'mission_time' in self.parameters:
                mission_time = self.parameters['mission_time']
                if mission_time != 'auto':
                    cmd.extend(['--mission-time', mission_time])
            
            if self.parameters.get('verbose_logging', False):
                cmd.append('--verbose')
            
            # Start the simulation process
            self.process = subprocess.Popen(
                cmd,
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
        log_dir.mkdir(exist_ok=True)
        
        # Monitor process output and log files
        last_log_pos = 0
        
        while self.running and self.process and self.process.poll() is None:
            # Look for log files
            if log_dir.exists():
                log_files = list(log_dir.glob("sls_simulation.log"))
                if log_files:
                    self.log_file_path = log_files[0]
            
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
        
        # Parse telemetry data - enhanced parsing
        telemetry_data = {}
        
        # Parse altitude
        altitude_match = re.search(r'Altitude[:\s]*(-?\d+\.?\d*)\s*m', line)
        if altitude_match:
            telemetry_data['altitude'] = float(altitude_match.group(1))
        
        # Parse velocity
        velocity_match = re.search(r'Velocity[:\s]*(-?\d+\.?\d*)\s*m/s', line)
        if velocity_match:
            telemetry_data['velocity'] = float(velocity_match.group(1))
            
        # Parse fuel remaining
        fuel_match = re.search(r'Fuel[:\s]*(\d+\.?\d*)%', line)
        if fuel_match:
            telemetry_data['fuel_remaining'] = float(fuel_match.group(1))
        
        if telemetry_data:
            telemetry_data['timestamp'] = datetime.now()
            self.telemetry_update.emit(telemetry_data)
        
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
        if "Mission phase changed" in line or "phase:" in line.lower():
            phase_match = re.search(r'phase[:\s]+(\w+)', line, re.IGNORECASE)
            if phase_match:
                phase_name = phase_match.group(1).upper()
                self.phase_update.emit(phase_name)


class TelemetryWidget(QGroupBox):
    """Enhanced widget for displaying real-time telemetry data."""
    
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
        
        # Fuel remaining
        layout.addWidget(QLabel("Fuel:"), 2, 0)
        self.fuel_label = QLabel("100.0%")
        self.fuel_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(self.fuel_label, 2, 1)
        
        self.fuel_progress = QProgressBar()
        self.fuel_progress.setMaximum(100)
        self.fuel_progress.setValue(100)
        layout.addWidget(self.fuel_progress, 2, 2)
        
        # Mission time
        layout.addWidget(QLabel("Mission Time:"), 3, 0)
        self.mission_time_label = QLabel("T-00:00:00")
        self.mission_time_label.setStyleSheet("font-weight: bold; font-size: 14px; color: #00ff00;")
        layout.addWidget(self.mission_time_label, 3, 1)
        
        # Current phase
        layout.addWidget(QLabel("Phase:"), 4, 0)
        self.phase_label = QLabel("PRE-LAUNCH")
        self.phase_label.setStyleSheet("font-weight: bold; font-size: 14px; color: #ffaa00;")
        layout.addWidget(self.phase_label, 4, 1)
        
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
        
        if 'velocity' in data:
            velocity = data['velocity']
            self.velocity_label.setText(f"{velocity:.1f} m/s")
            
        if 'fuel_remaining' in data:
            fuel = data['fuel_remaining']
            self.fuel_label.setText(f"{fuel:.1f}%")
            self.fuel_progress.setValue(int(fuel))
            
            # Color code fuel level
            if fuel < 20:
                self.fuel_progress.setStyleSheet("QProgressBar::chunk { background-color: #ff4444; }")
            elif fuel < 50:
                self.fuel_progress.setStyleSheet("QProgressBar::chunk { background-color: #ffaa44; }")
            else:
                self.fuel_progress.setStyleSheet("QProgressBar::chunk { background-color: #44ff44; }")
    
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


class EnhancedLaunchControlGUI(QMainWindow):
    """Enhanced main GUI application for the Space Launch System."""
    
    def __init__(self, development_mode=False):
        super().__init__()
        
        try:
            print("Initializing Enhanced GUI...")
            self.development_mode = development_mode
            
            # Find project root
            self.project_root = Path(__file__).parent.parent
            print(f"Project root: {self.project_root}")
            
            self.simulation_monitor = None
            self.settings = QSettings('QNX Aerospace', 'Space Launch System')
            
            print("Calling init_ui()...")
            self.init_ui()
            print("init_ui() completed successfully")
            
            print("Calling restore_settings()...")
            self.restore_settings()
            print("restore_settings() completed successfully")
            
        except Exception as e:
            print(f"Error during GUI initialization: {e}")
            import traceback
            traceback.print_exc()
            raise
        
    def init_ui(self):
        """Initialize the enhanced user interface."""
        self.setWindowTitle("QNX Space Launch System - Enhanced Mission Control")
        
        # Set window size
        self.resize(1400, 900)
        
        # Center the window on the screen
        screen = QApplication.primaryScreen()
        if screen:
            screen_geometry = screen.availableGeometry()
            print(f"Screen geometry: {screen_geometry}")
            # Calculate center position
            x = (screen_geometry.width() - 1400) // 2
            y = (screen_geometry.height() - 900) // 2
            # Ensure position is not negative
            x = max(x, 50)
            y = max(y, 50)
            print(f"Calculated position: ({x}, {y})")
            self.move(x, y)
        else:
            print("No screen found, using fallback position")
            # Fallback positioning
            self.move(50, 50)
        
        # Set application icon
        self.setWindowIcon(QIcon())  # You can add an icon file here
        
        # Create menu bar
        self.create_menu_bar()
        
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
            QTabWidget::pane {
                border: 1px solid #555555;
            }
            QTabBar::tab {
                background-color: #404040;
                color: #ffffff;
                border: 1px solid #555555;
                padding: 5px 10px;
                margin-right: 2px;
            }
            QTabBar::tab:selected {
                background-color: #555555;
            }
        """)
        
        # Create central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Main layout
        main_layout = QVBoxLayout(central_widget)
        
        # Header with title and mission status
        header_layout = QHBoxLayout()
        
        title_label = QLabel("🚀 QNX SPACE LAUNCH SYSTEM - ENHANCED")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #00aaff;")
        header_layout.addWidget(title_label)
        
        header_layout.addStretch()
        
        self.mission_status_label = QLabel("STANDBY")
        self.mission_status_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ffaa00;")
        header_layout.addWidget(self.mission_status_label)
        
        main_layout.addLayout(header_layout)
        
        # Control buttons
        button_layout = QHBoxLayout()
        
        self.configure_button = QPushButton("⚙️ CONFIGURE")
        self.configure_button.setStyleSheet("""
            QPushButton {
                background-color: #2d4a5a;
                font-weight: bold;
                font-size: 12px;
            }
            QPushButton:hover {
                background-color: #3d5a6a;
            }
        """)
        self.configure_button.clicked.connect(self.configure_mission)
        button_layout.addWidget(self.configure_button)
        
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
        
        # Create tab widget for different views
        self.tab_widget = QTabWidget()
        
        # Overview tab
        overview_tab = self.create_overview_tab()
        self.tab_widget.addTab(overview_tab, "📊 Overview")
        
        # Telemetry plots tab
        self.telemetry_plotter = TelemetryPlotter()
        self.tab_widget.addTab(self.telemetry_plotter, "📈 Telemetry Plots")
        
        # System monitor tab
        system_tab = self.create_system_tab()
        self.tab_widget.addTab(system_tab, "🖥️ System Monitor")
        
        # Logs tab
        logs_tab = self.create_logs_tab()
        self.tab_widget.addTab(logs_tab, "📝 Logs")
        
        main_layout.addWidget(self.tab_widget)
        
        # Status bar
        self.status_bar = QStatusBar()
        self.status_bar.showMessage("Ready to launch")
        if self.development_mode:
            self.status_bar.addPermanentWidget(QLabel("🔧 DEV MODE"))
        self.setStatusBar(self.status_bar)
        
    def create_menu_bar(self):
        """Create the application menu bar."""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu('&File')
        
        save_logs_action = QAction('&Save Logs...', self)
        save_logs_action.triggered.connect(self.save_logs)
        file_menu.addAction(save_logs_action)
        
        file_menu.addSeparator()
        
        exit_action = QAction('E&xit', self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # View menu
        view_menu = menubar.addMenu('&View')
        
        clear_logs_action = QAction('&Clear Logs', self)
        clear_logs_action.triggered.connect(self.clear_logs)
        view_menu.addAction(clear_logs_action)
        
        # Help menu
        help_menu = menubar.addMenu('&Help')
        
        about_action = QAction('&About', self)
        about_action.triggered.connect(self.show_about)
        help_menu.addAction(about_action)
    
    def create_overview_tab(self):
        """Create the overview tab."""
        widget = QWidget()
        layout = QHBoxLayout(widget)
        
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
        
        # Right panel - System monitor
        self.system_monitor_widget = SystemMonitorWidget()
        
        splitter.addWidget(left_panel)
        splitter.addWidget(self.system_monitor_widget)
        splitter.setSizes([600, 400])
        
        layout.addWidget(splitter)
        return widget
    
    def create_system_tab(self):
        """Create the system monitoring tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Additional system information would go here
        info_label = QLabel("Detailed system monitoring and performance metrics")
        info_label.setStyleSheet("font-size: 14px; color: #aaaaaa;")
        layout.addWidget(info_label)
        
        layout.addStretch()
        return widget
    
    def create_logs_tab(self):
        """Create the logs tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        self.log_widget = LogWidget()
        layout.addWidget(self.log_widget)
        
        return widget
    
    def configure_mission(self):
        """Open mission configuration dialog."""
        dialog = MissionParametersDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self.mission_parameters = dialog.get_parameters()
            self.status_bar.showMessage("Mission parameters configured")
        
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
        
        # Get mission parameters if configured
        parameters = getattr(self, 'mission_parameters', {})
        
        self.mission_status_label.setText("LAUNCHING...")
        self.mission_status_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ff6600;")
        
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.configure_button.setEnabled(False)
        
        # Create and start monitor thread
        self.simulation_monitor = SimulationMonitor(self.project_root, parameters)
        self.simulation_monitor.telemetry_update.connect(self.telemetry_widget.update_telemetry)
        self.simulation_monitor.telemetry_update.connect(self.telemetry_plotter.update_telemetry)
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
        self.configure_button.setEnabled(True)
        
        self.status_bar.showMessage("Simulation stopped")
    
    def save_logs(self):
        """Save current logs to file."""
        # Implementation for saving logs
        QMessageBox.information(self, "Save Logs", "Log saving functionality would be implemented here.")
    
    def clear_logs(self):
        """Clear all displayed logs."""
        reply = QMessageBox.question(self, 'Clear Logs',
                                   'Clear all displayed logs?',
                                   QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                                   QMessageBox.StandardButton.No)
        
        if reply == QMessageBox.StandardButton.Yes:
            self.log_widget.log_text.clear()
            self.status_widget.status_text.clear()
            self.telemetry_plotter.clear_data()
    
    def show_about(self):
        """Show about dialog."""
        QMessageBox.about(self, "About QNX Space Launch System",
                         "QNX Space Launch System GUI\n\n"
                         "A sophisticated simulation and monitoring interface\n"
                         "for space launch operations using QNX Neutrino RTOS.\n\n"
                         "Version 2.0\n"
                         "Built with PyQt6 and matplotlib")
    
    def restore_settings(self):
        """Restore application settings."""
        geometry = self.settings.value('geometry')
        if geometry:
            self.restoreGeometry(geometry)
    
    def save_settings(self):
        """Save application settings."""
        self.settings.setValue('geometry', self.saveGeometry())
        
    def closeEvent(self, event):
        """Handle application close."""
        if self.simulation_monitor and self.simulation_monitor.running:
            reply = QMessageBox.question(self, 'Close Application',
                                       'Simulation is running. Stop and exit?',
                                       QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                                       QMessageBox.StandardButton.No)
            
            if reply == QMessageBox.StandardButton.Yes:
                self.stop_simulation()
                self.save_settings()
                event.accept()
            else:
                event.ignore()
        else:
            self.save_settings()
            event.accept()


def main():
    """Main entry point."""
    try:
        # Parse command line arguments
        parser = argparse.ArgumentParser(description='QNX Space Launch System GUI')
        parser.add_argument('--development', action='store_true',
                           help='Run in development mode')
        args = parser.parse_args()
        
        print("Initializing QApplication...")
        app = QApplication(sys.argv)
        
        # Set application properties
        app.setApplicationName("QNX Space Launch System")
        app.setApplicationVersion("2.0")
        app.setOrganizationName("QNX Aerospace")
        
        # High DPI scaling is automatically handled in PyQt6
        # The AA_EnableHighDpiScaling attribute was removed in PyQt6
        
        # Create and show main window
        print("Creating GUI window...")
        window = EnhancedLaunchControlGUI(development_mode=args.development)
        print("GUI window created successfully")
        
        # Ensure window appears on screen and is visible
        print("Ensuring window is positioned correctly...")
        
        # Get screen information
        screen = app.primaryScreen()
        if screen:
            screen_geometry = screen.availableGeometry()
            print(f"Available screen geometry: {screen_geometry}")
        
        # Force a specific position that should be visible
        # Use screen-relative positioning
        if screen:
            screen_rect = screen.availableGeometry()
            # Position relative to screen's top-left
            x = screen_rect.x() + 100
            y = screen_rect.y() + 100
            window.move(x, y)
            print(f"Moved window to screen-relative position: ({x}, {y})")
        else:
            window.move(100, 100)
            print("Moved window to fallback position: (100, 100)")
        
        window.show()
        print("Called show()")
        
        # Force the window to be processed
        app.processEvents()
        
        window.raise_()  # Bring to front
        print("Called raise_()")
        
        window.activateWindow()  # Activate the window
        print("Called activateWindow()")
        
        # Try to force window to be visible
        window.setWindowState(window.windowState() & ~Qt.WindowState.WindowMinimized | Qt.WindowState.WindowActive)
        
        # Process events again
        app.processEvents()
        
        print("Window should now be visible")
        print(f"Window size: {window.size().width()}x{window.size().height()}")
        print(f"Window position: ({window.x()}, {window.y()})")
        print(f"Window is visible: {window.isVisible()}")
        print("Starting application event loop...")
        
        # Start the event loop
        result = app.exec()
        print(f"Application event loop finished with result: {result}")
        sys.exit(result)
        
    except Exception as e:
        print(f"Error occurred: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
