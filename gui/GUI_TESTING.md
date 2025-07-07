# QNX Space Launch System GUI Testing Guide

This document describes the comprehensive testing strategy and tools for the QNX Space Launch System GUI application.

## Test Suite Overview

The GUI test suite provides comprehensive coverage of the PyQt6-based launch control interface:

### Test Categories

1. **Unit Tests** - Individual component testing
   - Widget functionality
   - Data processing
   - Utility functions
   - System metrics

2. **Integration Tests** - Complete workflow testing
   - Mission lifecycle simulation
   - Data persistence and recovery
   - Multi-component interactions

3. **UI Interaction Tests** - User interface testing
   - Button clicks and menu actions
   - Tab switching
   - Dialog interactions
   - Keyboard and mouse events

4. **Performance Tests** - Speed and efficiency testing
   - Rapid telemetry updates
   - Plot rendering performance
   - Memory usage monitoring
   - Concurrent operations

5. **Stress Tests** - Reliability under load
   - High-frequency data updates
   - Long-running operations
   - Resource exhaustion scenarios
   - Error recovery

6. **Edge Case Tests** - Robustness testing
   - Invalid data handling
   - Missing files
   - Malformed inputs
   - Boundary conditions

## Running Tests

### Quick Start

```bash
# Run all tests with basic output
./run.sh --gui-test

# Run tests with coverage analysis
./run.sh --gui-test-coverage

# Use the enhanced test runner directly
cd gui
python run_tests.py --coverage --html-report
```

### Test Runner Options

The `run_tests.py` script provides extensive options:

```bash
# Basic test execution
python run_tests.py

# Coverage analysis with HTML reports
python run_tests.py --coverage --html-report

# Performance benchmarks only
python run_tests.py --benchmark

# Run specific test categories
python run_tests.py --unit-only
python run_tests.py --integration
python run_tests.py --stress

# Verbose output with parallel execution
python run_tests.py --verbose --parallel 4

# Filter tests by pattern
python run_tests.py --filter "test_telemetry"

# Install dependencies and run
python run_tests.py --install-deps --coverage
```

### Using GUI Launcher

The GUI launcher script supports multiple test modes:

```bash
cd gui

# Run standard test suite
./launch_gui.sh --test

# Run with coverage analysis
./launch_gui.sh --test-coverage

# Development mode with testing
./launch_gui.sh --dev --test
```

## Test Structure

### Test Files

- `test_gui.py` - Main test suite with all test categories
- `run_tests.py` - Advanced test runner with multiple options
- `pytest.ini` - PyTest configuration and markers

### Test Classes

1. **TestSystemMetrics** - System monitoring functionality
2. **TestEnhancedLaunchControlGUI** - Main GUI application
3. **TestTelemetryPlotter** - Telemetry visualization
4. **TestSystemMonitorWidget** - System performance monitoring
5. **TestSimulationMonitor** - Simulation process monitoring
6. **TestPerformanceAndStress** - Performance and stress testing
7. **TestUIInteractions** - User interface interactions
8. **TestErrorHandling** - Error scenarios and edge cases
9. **TestIntegrationWorkflows** - Complete mission workflows
10. **TestBenchmarks** - Performance benchmarking

### Test Fixtures

- `app` - QApplication instance for GUI testing
- `temp_project_dir` - Temporary project directory with test data

## Test Coverage

The test suite aims for comprehensive coverage:

- **Functional Coverage**: All major GUI features
- **Code Coverage**: >90% line coverage target
- **UI Coverage**: All interactive elements
- **Error Coverage**: Exception handling and edge cases

### Coverage Reports

Coverage reports are generated in multiple formats:

- **HTML Report**: `test_results/coverage/index.html`
- **Terminal Output**: Real-time coverage summary
- **XML Report**: `test_results/coverage.xml` (for CI/CD)

## Performance Benchmarks

### Benchmark Categories

1. **Telemetry Updates**: Real-time data processing speed
2. **Plot Rendering**: Chart update performance
3. **Memory Usage**: Resource consumption monitoring
4. **UI Responsiveness**: Interface interaction speed

### Performance Targets

- Telemetry updates: >100 updates/second
- Plot refresh: <5 seconds for 1000 data points
- Memory growth: <50MB over test duration
- UI response: <100ms for standard interactions

## Continuous Integration

### GitHub Actions Integration

Tests automatically run on:
- Pull requests to main branch
- Pushes to main branch
- Scheduled nightly runs

### CI Test Matrix

- Python 3.8, 3.9, 3.10, 3.11
- Ubuntu 20.04, 22.04
- With and without GUI display (Xvfb)

## Debugging Tests

### Running Individual Tests

```bash
# Run specific test method
python -m pytest test_gui.py::TestSystemMetrics::test_initialization -v

# Run specific test class
python -m pytest test_gui.py::TestTelemetryPlotter -v

# Run with debugger on failure
python -m pytest test_gui.py --pdb
```

### Test Debugging Tips

1. **Use verbose output** (`-v`) to see detailed test names
2. **Enable print statements** with `-s` flag
3. **Run tests individually** when debugging specific issues
4. **Use mock debugging** to inspect mock calls
5. **Check test fixtures** for proper setup/teardown

### Common Issues

1. **Display Issues**: Use Xvfb for headless testing
2. **Timing Issues**: Add appropriate waits for UI updates
3. **Resource Cleanup**: Ensure proper widget cleanup in tests
4. **Mock Configuration**: Verify mock objects match real interfaces

## Test Data

### Mock Data Generation

Tests use realistic mock data:

- Telemetry data with proper ranges and units
- Mission phases with appropriate transitions
- System metrics reflecting real system behavior
- Log entries with correct formatting

### Test Scenarios

1. **Normal Mission**: Complete successful launch sequence
2. **Abort Scenarios**: Mission abort at various phases
3. **System Failures**: Component failures and recovery
4. **Data Loss**: Handling of missing or corrupted data
5. **Performance Stress**: High-load operational scenarios

## Extending Tests

### Adding New Tests

1. **Follow naming conventions**: `test_feature_description`
2. **Use appropriate markers**: `@pytest.mark.unit`, etc.
3. **Include docstrings**: Describe what the test validates
4. **Mock external dependencies**: Use `@patch` for system calls
5. **Clean up resources**: Properly close widgets and processes

### Test Best Practices

1. **Atomic Tests**: Each test should test one specific behavior
2. **Independent Tests**: Tests should not depend on each other
3. **Deterministic Results**: Tests should produce consistent results
4. **Clear Assertions**: Use descriptive assertion messages
5. **Proper Mocking**: Mock at the right boundary level

## Reporting Issues

When tests fail:

1. **Check test output** for specific error messages
2. **Review coverage reports** for missed code paths
3. **Run tests individually** to isolate issues
4. **Check system requirements** (Python version, dependencies)
5. **Verify GUI environment** (display, window manager)

## Future Enhancements

Planned test improvements:

1. **Visual Regression Testing**: Screenshot comparison
2. **Load Testing**: Extended stress scenarios
3. **Cross-Platform Testing**: Windows and macOS support
4. **Accessibility Testing**: Screen reader and keyboard navigation
5. **Internationalization Testing**: Multi-language support
