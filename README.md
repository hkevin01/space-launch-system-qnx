# QNX-Based Space Launch System Simulation

A high-fidelity real-time simulation of a space launch system using QNX Neutrino RTOS, designed to emulate real-time conditions, subsystem communication, and fault-tolerant behavior for pre-launch and ascent operations.

## 🚀 Project Overview

This project simulates the key components of a space launch platform including:

- **Flight Control Computer (FCC)** - Primary flight control and navigation
- **Engine Control System** - Propulsion management and throttle control
- **Telemetry & Communications** - Data collection and ground communication
- **Environmental Monitoring** - Temperature, pressure, and vibration sensors
- **Ground Support Interface** - Mission control integration

## 🏗️ Architecture

The system leverages QNX Neutrino RTOS features:

- **Microkernel Architecture** - Inter-process communication (IPC) among subsystems
- **Resource Managers** - Priority scheduling for real-time constraints
- **Fault Isolation** - Redundant system design for safety-critical operations
- **Deterministic Execution** - Predictable timing for mission-critical tasks

## 📁 Project Structure

```
├── src/                    # Source code
│   ├── subsystems/        # Individual subsystem implementations
│   ├── common/            # Shared utilities and data structures
│   ├── ui/                # User interface and dashboard
│   └── main.c             # Main application entry point
├── tests/                 # Unit and integration tests
├── config/                # Configuration files
├── scripts/               # Build and deployment scripts
├── docs/                  # Documentation
├── .github/               # GitHub workflows
└── .copilot/              # Copilot configuration
```

## 🛠️ Building the Project

### Prerequisites

- QNX Software Development Platform 7.1+
- QNX Neutrino RTOS target system
- GCC toolchain for QNX
- Make utilities

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd space-launch-system-qnx

# Build the project
make all

# Run simulation (terminal mode)
make run

# Run with Enhanced GUI
./run.sh --gui

# Run with Basic GUI
./run.sh --gui-basic

# Run GUI in development mode
./run.sh --gui-dev

# Run tests
make test

# Run GUI tests with coverage
./run.sh --gui-test-coverage
```

## 🖥️ User Interfaces

### Enhanced GUI (Recommended)

The enhanced GUI provides a modern PyQt6 interface with:

- **Real-time Telemetry Plotting** - Live charts for altitude, velocity, acceleration
- **System Performance Monitoring** - CPU, memory, and disk usage visualization  
- **Mission Parameter Configuration** - Interactive mission setup dialogs
- **Tabbed Interface** - Organized views for different operational aspects
- **Advanced Controls** - Start/stop simulation, parameter adjustment

Launch with: `./run.sh --gui` or `./run.sh --gui-enhanced`

### Basic GUI

A simplified interface for basic operations:

- Essential telemetry display
- Start/stop controls
- Basic mission monitoring

Launch with: `./run.sh --gui-basic`

### Terminal Interface

Traditional command-line interface for:

- Automated operations
- Debugging and development
- Headless environments

Launch with: `./run.sh` (default mode)

## 🎯 Features

### Core Simulation Features

- **Launch Profile Simulation** - Fuel pressurization, hold/release logic, throttle ramps
- **Sensor Data Streams** - Real-time temperature, vibration, acceleration data
- **Mission Control Interface** - Go/No-Go logic, abort sequences
- **Real-time Dashboard** - Live telemetry visualization

### Safety & Redundancy

- **Fail-safe Behaviors** - Engine cutoff, system fallback mechanisms
- **Fault Injection** - Simulate system failures for testing
- **Deterministic Execution** - Guaranteed response times under load

### Optional Enhancements

- **AI Anomaly Detection** - Machine learning for failure pattern recognition
- **Hardware Interface Simulation** - CAN bus and serial communication emulation
- **cFS Compatibility** - Core Flight System integration layer

## 📊 Telemetry & Monitoring

The simulation provides comprehensive telemetry data:

- Real-time system status
- Performance metrics
- Fault detection and reporting
- Historical data logging
- OGC-standard telemetry formatting

## 🔧 Configuration

System parameters can be configured via:

- `config/system.conf` - System-wide settings
- `config/subsystems/` - Individual subsystem configurations
- Environment variables for runtime parameters

## 🧪 Testing

The project includes comprehensive testing at multiple levels:

### Core System Tests
- Unit tests for individual components
- Integration tests for subsystem communication
- Stress tests for real-time performance
- Fault injection tests for safety validation

### GUI Testing Suite

The GUI has its own comprehensive test suite with:

- **Unit Tests** - Individual widget and component testing
- **Integration Tests** - Complete workflow validation
- **UI Interaction Tests** - Button clicks, menu actions, dialogs
- **Performance Tests** - Real-time update speed and efficiency
- **Stress Tests** - High-load scenarios and concurrent operations
- **Error Handling Tests** - Invalid data and edge case handling

#### Running GUI Tests

```bash
# Run all GUI tests
./run.sh --gui-test

# Run with coverage analysis and HTML reports
./run.sh --gui-test-coverage

# Use advanced test runner with options
cd gui
python run_tests.py --coverage --html-report --benchmark

# Run specific test categories
python run_tests.py --unit-only
python run_tests.py --integration
python run_tests.py --stress
```

#### Test Reports

Tests generate comprehensive reports:
- **HTML Test Report**: `gui/test_results/test_report.html`
- **Coverage Report**: `gui/test_results/coverage/index.html`
- **Performance Benchmarks**: Integrated in test output

See [GUI Testing Guide](gui/GUI_TESTING.md) for detailed information.

## 📚 Documentation

Comprehensive documentation is available in the `docs/` directory:

### Core Documentation
- [System Design](docs/system-design.md) - Overall architecture and design principles
- [User Guide](docs/user-guide.md) - Getting started and operation procedures
- [Simulation Architecture](docs/simulation-architecture.md) - Technical architecture details

### QNX-Specific Documentation
- [QNX Simplified Guide](docs/qnx-simplified.md) - Easy-to-understand explanation of QNX
- [QNX Overview](docs/qnx-overview.md) - Why QNX for aerospace applications
- [QNX Implementation](docs/qnx-implementation.md) - How QNX is used in this project
- [QNX Simulation Implementation](docs/qnx-simulation-implementation.md) - How our simulation uses QNX technology
- [QNX Real-Time Features](docs/qnx-realtime-features.md) - Real-time capabilities and timing
- [QNX Integration Guide](docs/qnx-integration.md) - Complete system integration details
- [QNX Deployment Guide](docs/qnx-deployment.md) - Production deployment procedures

### Safety and Operations
- [Safety & Fault Tolerance](docs/safety-and-fault-tolerance.md) - Safety-critical design patterns

## 🤝 Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct and the process for submitting pull requests.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- QNX Software Systems for the robust RTOS platform
- NASA for space systems design inspiration
- Open source aerospace community

## 🔗 Related Projects

- [Core Flight System (cFS)](https://github.com/nasa/cFS)
- [F' Flight Software Framework](https://github.com/nasa/fprime)
- [COSMOS](https://cosmosrb.com/) - Mission Control Software

---

**Note**: This is a simulation project for educational and development purposes. It is not intended for actual spacecraft operations.
