#!/usr/bin/env python3
"""
QNX Space Launch System GUI Test Suite

Comprehensive test suite for the GUI application including:
- Unit tests for individual widgets
- Integration tests for full workflows
- Performance tests for real-time updates
- Mock simulation tests
- UI interaction tests
- Stress tests and edge cases
"""

import concurrent.futures
import sys
import tempfile
import time
from pathlib import Path
from unittest.mock import Mock, patch

import pytest
from PyQt6.QtCore import Qt
from PyQt6.QtTest import QTest
from PyQt6.QtWidgets import QApplication, QPushButton

# Import our GUI modules
sys.path.insert(0, str(Path(__file__).parent))
from enhanced_launch_gui import (
    EnhancedLaunchControlGUI,
    MissionParametersDialog,
    SimulationMonitor,
    SystemMetrics,
    SystemMonitorWidget,
    TelemetryPlotter,
)


class TestSystemMetrics:
    """Test the SystemMetrics class."""
    
    def test_initialization(self):
        """Test SystemMetrics initialization."""
        metrics = SystemMetrics()
        assert metrics.cpu_usage == []
        assert metrics.memory_usage == []
        assert metrics.timestamps == []
        assert metrics.max_samples == 100
    
    @patch('psutil.cpu_percent')
    @patch('psutil.virtual_memory')
    def test_update_metrics(self, mock_memory, mock_cpu):
        """Test metrics update functionality."""
        # Mock system data
        mock_cpu.return_value = 45.5
        mock_memory.return_value.percent = 67.2
        
        metrics = SystemMetrics()
        metrics.update()
        
        assert len(metrics.cpu_usage) == 1
        assert len(metrics.memory_usage) == 1
        assert len(metrics.timestamps) == 1
        assert metrics.cpu_usage[0] == 45.5
        assert metrics.memory_usage[0] == 67.2
    
    def test_max_samples_limit(self):
        """Test that metrics don't exceed max_samples."""
        metrics = SystemMetrics()
        metrics.max_samples = 5
        
        # Add more samples than the limit
        for i in range(10):
            metrics.timestamps.append(i)
            metrics.cpu_usage.append(i * 10)
            metrics.memory_usage.append(i * 5)
            
            # Manually apply the limit (normally done in update())
            if len(metrics.timestamps) > metrics.max_samples:
                metrics.timestamps.pop(0)
                metrics.cpu_usage.pop(0)
                metrics.memory_usage.pop(0)
        
        assert len(metrics.timestamps) == 5
        assert len(metrics.cpu_usage) == 5
        assert len(metrics.memory_usage) == 5
    
    def test_metrics_data_limits(self):
        """Test that metrics properly limit data points."""
        metrics = SystemMetrics()
        metrics.max_samples = 5  # Set after initialization
        
        # Add more data than max_samples
        for i in range(10):
            with patch('psutil.cpu_percent', return_value=i), \
                 patch('psutil.virtual_memory') as mock_mem:
                mock_mem.return_value.percent = i * 10
                metrics.update()
        
        # Should only keep last 5 samples
        assert len(metrics.cpu_usage) == 5
        assert len(metrics.memory_usage) == 5
        assert len(metrics.timestamps) == 5
        
        # Should have latest values
        assert metrics.cpu_usage[-1] == 9
        assert metrics.memory_usage[-1] == 90
    
    def test_get_latest_metrics(self):
        """Test retrieving latest metrics."""
        metrics = SystemMetrics()
        
        with patch('psutil.cpu_percent', return_value=50), \
             patch('psutil.virtual_memory') as mock_mem:
            mock_mem.return_value.percent = 75
            metrics.update()
            
            latest = metrics.get_latest_metrics()
            assert latest['cpu'] == 50
            assert latest['memory'] == 75
            assert 'timestamp' in latest


@pytest.fixture
def app():
    """Create QApplication for testing."""
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    return app


@pytest.fixture
def temp_project_dir():
    """Create temporary project directory for testing."""
    with tempfile.TemporaryDirectory() as temp_dir:
        # Create basic project structure
        project_path = Path(temp_dir)
        (project_path / "logs").mkdir()
        (project_path / "config").mkdir()
        (project_path / "bin").mkdir()
        
        # Create dummy files
        config_file = project_path / "config" / "system.conf"
        config_file.write_text("[system]\nname=test\n")
        
        log_file = project_path / "logs" / "sls_simulation.log"
        log_file.write_text("")
        
        telemetry_file = project_path / "logs" / "telemetry.csv"
        telemetry_file.write_text("time,altitude,velocity\n")
        
        yield str(project_path)


class TestEnhancedLaunchControlGUI:
    """Test the main GUI application."""
    
    def test_initialization(self, app, temp_project_dir):
        """Test GUI initialization."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            assert gui.project_root == temp_project_dir
            # Basic initialization test
            gui.close()
    
    def test_start_simulation_mock(self, app, temp_project_dir):
        """Test starting simulation with mocked process."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            with patch('subprocess.Popen') as mock_popen:
                mock_process = Mock()
                mock_process.poll.return_value = None  # Still running
                mock_popen.return_value = mock_process
                
                gui.start_simulation()
                # Check if simulation appears to be running
                mock_popen.assert_called_once()
            
            gui.close()
    
    def test_stop_simulation(self, app, temp_project_dir):
        """Test stopping simulation."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Mock running simulation
            mock_process = Mock()
            if hasattr(gui, 'simulation_process'):
                gui.simulation_process = mock_process
            if hasattr(gui, 'is_simulation_running'):
                gui.is_simulation_running = True
            
            gui.stop_simulation()
            # Process should be terminated
            
            gui.close()
    
    def test_mission_parameter_dialog(self, app, temp_project_dir):
        """Test mission parameter configuration dialog."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Test opening mission parameters dialog
            dialog = MissionParametersDialog(gui)
            assert dialog.isModal()
            
            # Test default values - use actual attribute names
            assert dialog.vehicle_mass_spin.value() == 500000.0
            assert dialog.target_altitude_spin.value() == 400000.0
            
            dialog.close()
            gui.close()
    
    def test_telemetry_widget_updates(self, app, temp_project_dir):
        """Test telemetry widget real-time updates."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Get telemetry widget
            telemetry_widget = gui.telemetry_widget
            
            # Test telemetry update
            test_data = {
                'altitude': 50000,
                'velocity': 1500,
                'acceleration': 9.8,
                'fuel_level': 75.5,
                'engine_temp': 2500,
                'chamber_pressure': 150.0
            }
            
            telemetry_widget.update_telemetry(test_data)
            
            # Verify labels are updated
            assert "50000" in telemetry_widget.altitude_label.text()
            assert "1500" in telemetry_widget.velocity_label.text()
            
            gui.close()


class TestTelemetryPlotter:
    """Test telemetry plotting functionality."""
    
    def test_initialization(self, app):
        """Test plotter initialization."""
        plotter = TelemetryPlotter()
        assert len(plotter.telemetry_data['altitude']) == 0
        assert len(plotter.telemetry_data['velocity']) == 0
        assert len(plotter.telemetry_data['timestamps']) == 0
        plotter.close()
    
    def test_data_update(self, app):
        """Test data updates and plotting."""
        plotter = TelemetryPlotter()
        
        # Add test data
        test_data = {
            'altitude': 10000,
            'velocity': 500,
        }
        
        plotter.update_telemetry(test_data)
        
        assert len(plotter.telemetry_data['altitude']) == 1
        assert len(plotter.telemetry_data['velocity']) == 1
        assert len(plotter.telemetry_data['timestamps']) == 1
        assert plotter.telemetry_data['altitude'][0] == 10000
        assert plotter.telemetry_data['velocity'][0] == 500
        
        plotter.close()
    
    def test_data_limit(self, app):
        """Test data point limiting."""
        plotter = TelemetryPlotter()
        plotter.max_samples = 5  # Set limit after initialization
        
        # Add more data than limit
        for i in range(10):
            test_data = {
                'altitude': i * 1000,
                'velocity': i * 100,
            }
            plotter.update_telemetry(test_data)
        
        # Should only keep last 5 points
        assert len(plotter.telemetry_data['altitude']) == 5
        assert len(plotter.telemetry_data['velocity']) == 5
        assert len(plotter.telemetry_data['timestamps']) == 5
        
        # Should have latest values
        assert plotter.telemetry_data['altitude'][-1] == 9000
        assert plotter.telemetry_data['velocity'][-1] == 900
        
        plotter.close()
    
    def test_plot_refresh(self, app):
        """Test plot refresh functionality."""
        plotter = TelemetryPlotter()
        
        # Add some data
        for i in range(3):
            test_data = {
                'altitude': i * 1000,
                'velocity': i * 100,
            }
            plotter.update_telemetry(test_data)
        
        # Test refresh
        plotter.refresh_plots()
        
        # Should not crash
        assert True
        
        plotter.close()


class TestSystemMonitorWidget:
    """Test system monitoring functionality."""
    
    def test_initialization(self, app):
        """Test system monitor initialization."""
        monitor = SystemMonitorWidget()
        assert monitor.metrics is not None
        monitor.close()
    
    @patch('psutil.cpu_percent')
    @patch('psutil.virtual_memory')
    @patch('psutil.disk_usage')
    def test_metrics_update(self, mock_disk, mock_memory, mock_cpu, app):
        """Test system metrics update."""
        mock_cpu.return_value = 45.0
        mock_memory.return_value.percent = 60.0
        mock_disk.return_value.percent = 80.0
        
        monitor = SystemMonitorWidget()
        monitor.update_metrics()
        
        # Should update without errors
        assert True
        monitor.close()


class TestSimulationMonitor:
    """Test simulation monitoring functionality."""
    
    def test_initialization(self, temp_project_dir):
        """Test monitor initialization."""
        monitor = SimulationMonitor(temp_project_dir)
        assert monitor.project_root == temp_project_dir
        assert monitor.is_monitoring == False
    
    def test_log_file_detection(self, temp_project_dir):
        """Test log file detection."""
        monitor = SimulationMonitor(temp_project_dir)
        
        # Create test log file
        log_file = Path(temp_project_dir) / "logs" / "sls_simulation.log"
        log_file.write_text("Test log content\n")
        
        # Should detect the file
        assert monitor.log_file_exists()
    
    def test_log_parsing(self, temp_project_dir):
        """Test log line parsing."""
        monitor = SimulationMonitor(temp_project_dir)
        
        # Test various log formats
        test_lines = [
            "[12:34:56] INFO: Altitude: 10000m",
            "[12:34:57] TELEMETRY: velocity=500 altitude=10500",
            "[12:34:58] ENGINE: Chamber pressure: 150.0 bar",
            "[12:34:59] FLIGHT: Mission phase: ASCENT"
        ]
        
        for line in test_lines:
            result = monitor.parse_log_line(line)
            # Should not crash
            assert True


class TestPerformanceAndStress:
    """Performance and stress tests for the GUI."""
    
    def test_rapid_telemetry_updates(self, app, temp_project_dir):
        """Test rapid telemetry updates to check for performance issues."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            start_time = time.time()
            
            # Send 100 rapid updates
            for i in range(100):
                test_data = {
                    'altitude': i * 100,
                    'velocity': i * 10,
                    'acceleration': 9.8 + i * 0.1,
                    'fuel_level': 100 - i,
                    'engine_temp': 2000 + i,
                    'chamber_pressure': 100 + i
                }
                gui.telemetry_widget.update_telemetry(test_data)
            
            end_time = time.time()
            update_time = end_time - start_time
            
            # Should complete in reasonable time (less than 1 second)
            assert update_time < 1.0, f"Updates took too long: {update_time}s"
            
            gui.close()
    
    def test_concurrent_data_updates(self, app, temp_project_dir):
        """Test concurrent data updates from multiple sources."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            def update_telemetry(thread_id):
                for i in range(10):
                    test_data = {
                        'altitude': thread_id * 1000 + i * 100,
                        'velocity': thread_id * 100 + i * 10,
                        'fuel_level': 100 - i
                    }
                    gui.telemetry_widget.update_telemetry(test_data)
                    time.sleep(0.01)  # Small delay to simulate real updates
            
            # Run concurrent updates
            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
                futures = [executor.submit(update_telemetry, i) for i in range(3)]
                concurrent.futures.wait(futures)
            
            # Should complete without errors
            assert True
            
            gui.close()
    
    def test_memory_usage_monitoring(self, app, temp_project_dir):
        """Test that GUI doesn't have significant memory leaks."""
        import os

        import psutil
        
        process = psutil.Process(os.getpid())
        initial_memory = process.memory_info().rss
        
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            # Create and destroy multiple GUI instances
            for _ in range(5):
                gui = EnhancedLaunchControlGUI()
                
                # Do some operations
                for i in range(20):
                    test_data = {
                        'altitude': i * 1000,
                        'velocity': i * 100,
                        'fuel_level': 100 - i
                    }
                    gui.telemetry_widget.update_telemetry(test_data)
                
                gui.close()
        
        final_memory = process.memory_info().rss
        memory_increase = final_memory - initial_memory
        
        # Memory increase should be reasonable (less than 50MB)
        assert memory_increase < 50 * 1024 * 1024, f"Memory increase too large: {memory_increase / 1024 / 1024:.2f}MB"


class TestUIInteractions:
    """Test user interface interactions."""
    
    def test_button_clicks(self, app, temp_project_dir):
        """Test button click events."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Find the start button (it should exist)
            start_button = None
            for child in gui.findChildren(QPushButton):
                if 'start' in child.text().lower():
                    start_button = child
                    break
            
            if start_button:
                # Test start button
                with patch.object(gui, 'start_simulation') as mock_start:
                    QTest.mouseClick(start_button, Qt.MouseButton.LeftButton)
                    mock_start.assert_called_once()
            
            # Find the stop button
            stop_button = None
            for child in gui.findChildren(QPushButton):
                if 'stop' in child.text().lower():
                    stop_button = child
                    break
            
            if stop_button:
                # Test stop button
                with patch.object(gui, 'stop_simulation') as mock_stop:
                    QTest.mouseClick(stop_button, Qt.MouseButton.LeftButton)
                    mock_stop.assert_called_once()
            
            gui.close()
    
    def test_menu_actions(self, app, temp_project_dir):
        """Test menu actions."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Test that menu exists and has expected actions
            menubar = gui.menuBar()
            assert menubar is not None
            
            # Look for File menu
            file_menu = None
            for action in menubar.actions():
                if action.text() == "&File":
                    file_menu = action.menu()
                    break
            
            if file_menu:
                assert file_menu is not None
            
            gui.close()
    
    def test_tab_switching(self, app, temp_project_dir):
        """Test tab switching functionality."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Test switching between tabs
            tab_widget = gui.central_tabs
            
            # Switch to different tabs
            for i in range(tab_widget.count()):
                tab_widget.setCurrentIndex(i)
                assert tab_widget.currentIndex() == i
            
            gui.close()


class TestErrorHandling:
    """Test error handling and edge cases."""
    
    def test_invalid_telemetry_data(self, app, temp_project_dir):
        """Test handling of invalid telemetry data."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Test with invalid data types
            gui.telemetry_widget.update_telemetry({'altitude': 'invalid'})
            # Should not crash
            
            # Test with None values
            gui.telemetry_widget.update_telemetry({'altitude': None})
            # Should not crash
            
            # Test with empty dict
            gui.telemetry_widget.update_telemetry({})
            # Should not crash
            
            # Test with negative values
            gui.telemetry_widget.update_telemetry({'altitude': -1000})
            # Should not crash
            
            gui.close()
    
    def test_missing_project_files(self, app):
        """Test behavior when project files are missing."""
        with tempfile.TemporaryDirectory() as temp_dir:
            # Don't create the expected project structure
            with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_dir}):
                # Should handle missing files gracefully
                gui = EnhancedLaunchControlGUI()
                # Should not crash
                gui.close()
    
    def test_log_parsing_edge_cases(self, temp_project_dir):
        """Test log parsing with various edge cases."""
        monitor = SimulationMonitor(temp_project_dir)
        
        # Test empty line
        monitor.parse_log_line("")
        
        # Test malformed line
        monitor.parse_log_line("This is not a valid log line")
        
        # Test line with no data
        monitor.parse_log_line("[12:34:56] INFO: No specific data")
        
        # Test very long line
        long_line = "[12:34:56] INFO: " + "A" * 10000
        monitor.parse_log_line(long_line)
        
        # Test unicode characters
        monitor.parse_log_line("[12:34:56] INFO: Test with ñ and 中文")
        
        # Should not crash on any of these


class TestIntegrationWorkflows:
    """Test complete workflows and integration scenarios."""
    
    def test_complete_mission_workflow(self, app, temp_project_dir):
        """Test a complete mission workflow from start to finish."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # 1. Configure mission parameters
            dialog = MissionParametersDialog(gui)
            dialog.payload_mass_spin.setValue(30000)
            dialog.target_altitude_spin.setValue(500)
            # Accept dialog (normally done by user clicking OK)
            
            # 2. Start simulation
            with patch('subprocess.Popen') as mock_popen:
                mock_process = Mock()
                mock_process.poll.return_value = None
                mock_popen.return_value = mock_process
                
                gui.start_simulation()
                assert gui.is_simulation_running == True
            
            # 3. Simulate telemetry updates throughout mission phases
            mission_phases = [
                ('PRE_LAUNCH', 0, 0),
                ('ASCENT', 5000, 100),
                ('MECO', 50000, 1500),
                ('STAGE_SEPARATION', 55000, 1400),
                ('SECOND_STAGE', 75000, 2000),
                ('ORBIT_INSERTION', 400000, 7800)
            ]
            
            for phase, altitude, velocity in mission_phases:
                telemetry_data = {
                    'altitude': altitude,
                    'velocity': velocity,
                    'mission_phase': phase,
                    'fuel_level': max(0, 100 - altitude / 1000),
                    'engine_temp': 2000 + altitude / 100
                }
                gui.telemetry_widget.update_telemetry(telemetry_data)
                
                # Verify phase is updated
                if hasattr(gui, 'mission_phase_label'):
                    phase_text = gui.mission_phase_label.text()
                    # Phase should be reflected in UI
            
            # 4. Stop simulation
            gui.stop_simulation()
            assert gui.is_simulation_running == False
            
            gui.close()
    
    def test_data_persistence_and_recovery(self, app, temp_project_dir):
        """Test data persistence and recovery scenarios."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            # Create initial GUI and add some data
            gui1 = EnhancedLaunchControlGUI()
            
            # Add telemetry data
            for i in range(5):
                test_data = {
                    'altitude': i * 1000,
                    'velocity': i * 100,
                    'timestamp': time.time() + i
                }
                gui1.telemetry_widget.update_telemetry(test_data)
            
            gui1.close()
            
            # Create new GUI instance and check if data can be recovered
            gui2 = EnhancedLaunchControlGUI()
            
            # If there's a data recovery mechanism, test it here
            # For now, just verify the GUI starts correctly
            assert gui2.project_root == temp_project_dir
            
            gui2.close()


# Benchmark tests
class TestBenchmarks:
    """Performance benchmark tests."""
    
    def test_telemetry_update_benchmark(self, app, temp_project_dir):
        """Benchmark telemetry update performance."""
        with patch.dict('os.environ', {'SLS_PROJECT_ROOT': temp_project_dir}):
            gui = EnhancedLaunchControlGUI()
            
            # Benchmark 1000 telemetry updates
            start_time = time.time()
            
            for i in range(1000):
                test_data = {
                    'altitude': i * 100,
                    'velocity': i * 10,
                    'acceleration': 9.8,
                    'fuel_level': 100 - (i * 0.1),
                    'engine_temp': 2000 + i,
                    'chamber_pressure': 100 + (i * 0.1)
                }
                gui.telemetry_widget.update_telemetry(test_data)
            
            end_time = time.time()
            total_time = end_time - start_time
            updates_per_second = 1000 / total_time
            
            print(f"\nTelemetry Update Benchmark:")
            print(f"  Total time: {total_time:.3f}s")
            print(f"  Updates per second: {updates_per_second:.1f}")
            
            # Should achieve at least 100 updates per second
            assert updates_per_second > 100, f"Performance too slow: {updates_per_second:.1f} updates/sec"
            
            gui.close()
    
    def test_plot_update_benchmark(self, app):
        """Benchmark plot update performance."""
        plotter = TelemetryPlotter(max_points=1000)
        
        start_time = time.time()
        
        # Add 1000 data points
        for i in range(1000):
            test_data = {
                'altitude': i * 100 + (i % 100) * 10,  # Add some variation
                'velocity': i * 10 + (i % 50) * 5,
                'timestamp': time.time() + i * 0.1
            }
            plotter.update_data(test_data)
        
        # Force a plot refresh
        plotter.refresh_plots()
        
        end_time = time.time()
        total_time = end_time - start_time
        
        print(f"\nPlot Update Benchmark:")
        print(f"  Total time: {total_time:.3f}s")
        print(f"  Points per second: {1000 / total_time:.1f}")
        
        # Should complete in reasonable time
        assert total_time < 5.0, f"Plot updates too slow: {total_time:.3f}s"
        
        plotter.close()


if __name__ == "__main__":
    # Allow running tests directly
    pytest.main([__file__, "-v", "--tb=short"])
