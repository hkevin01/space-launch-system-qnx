# User Guide

## QNX Space Launch System Simulation

### Getting Started

This guide will help you get started with the QNX Space Launch System simulation.

### Prerequisites

Before running the simulation, ensure you have:

1. **QNX Software Development Platform 7.1+**
   - QNX Neutrino RTOS
   - QNX Momentics IDE (optional)
   - GCC toolchain for QNX

2. **System Requirements**
   - Minimum 4 GB RAM
   - 1 GB free disk space
   - Multi-core processor recommended

3. **Development Tools**
   - Make utilities
   - Git (for version control)
   - Text editor or IDE

### Installation

1. **Clone the Repository**
   ```bash
   git clone <repository-url>
   cd space-launch-system-qnx
   ```

2. **Set up QNX Environment**
   ```bash
   source /opt/qnx710/qnxsdp-env.sh
   ```

3. **Build the Project**
   ```bash
   ./scripts/build.sh
   ```

### Running the Simulation

#### Basic Operation

1. **Start the Simulation**
   ```bash
   ./scripts/run_simulation.sh
   ```

2. **Monitor Output**
   The simulation will display real-time status information:
   ```
   [12:34:56.789] INFO  FCC         : Flight Control Computer started
   [12:34:56.790] INFO  ECS         : Engine Control System started
   [12:34:56.791] INFO  TELEM       : Telemetry system started
   ```

3. **Stop the Simulation**
   Press `Ctrl+C` to gracefully stop the simulation.

#### Advanced Options

1. **Custom Mission Time**
   ```bash
   ./scripts/run_simulation.sh t-600    # Start at T-10 minutes
   ./scripts/run_simulation.sh now      # Start at T-0 (immediate launch)
   ```

2. **Debug Mode**
   ```bash
   ./scripts/run_simulation.sh auto "" true
   ```

3. **Custom Configuration**
   ```bash
   ./scripts/run_simulation.sh auto config/custom.conf
   ```

### Understanding the Output

#### Log Levels

- **DEBUG**: Detailed diagnostic information
- **INFO**: General system information
- **WARN**: Warning conditions that don't stop operation
- **ERROR**: Error conditions that may affect operation
- **CRIT**: Critical conditions requiring immediate attention

#### Mission Phases

The simulation progresses through several mission phases:

1. **Pre-launch** (T-2 hours to T-6 seconds)
   - System initialization
   - Pre-flight checks
   - Hold point management

2. **Ignition** (T-6 to T-0 seconds)
   - Engine startup sequence
   - Final systems checks
   - Commit to launch

3. **Liftoff** (T+0 to T+10 seconds)
   - Vehicle departs launch pad
   - Initial vertical ascent
   - Tower clearance

4. **Ascent** (T+10 seconds to T+2 minutes)
   - Gravity turn maneuver
   - Maximum dynamic pressure
   - Atmospheric flight

5. **Stage Separation** (T+2 minutes)
   - First stage shutdown
   - Stage separation event
   - Second stage ignition

6. **Orbit Insertion** (T+2 to T+8 minutes)
   - Orbital velocity achievement
   - Trajectory corrections
   - Mission completion

#### Telemetry Data

Key telemetry parameters include:

- **Vehicle State**
  - Altitude (meters)
  - Velocity (m/s)
  - Acceleration (m/s²)
  - Fuel remaining (%)

- **Engine Parameters**
  - Thrust percentage (%)
  - Chamber pressure (Pa)
  - Fuel flow rate (kg/s)
  - Nozzle temperature (K)

- **System Health**
  - Subsystem status
  - Fault conditions
  - Communication status
  - Power consumption

### Configuration

#### System Configuration File

The main configuration file is located at `config/system.conf`. Key parameters:

```ini
[system]
simulation_rate_hz = 100
log_level = INFO

[vehicle]
dry_mass_kg = 500000
fuel_mass_kg = 1500000
max_thrust_n = 7500000

[mission]
target_altitude_m = 400000
launch_azimuth_deg = 90.0
```

#### Environment Variables

- `SLS_CONFIG_FILE`: Path to configuration file
- `SLS_LOG_DIR`: Directory for log files
- `SLS_CONFIG_OVERRIDE`: Configuration parameter overrides

### Monitoring and Analysis

#### Log Files

The simulation generates several log files:

- `logs/sls_simulation.log`: Main system log
- `logs/telemetry.csv`: Telemetry data in CSV format
- `logs/mission_events.log`: Mission milestone events

#### Real-time Monitoring

Monitor the simulation in real-time:

```bash
# Watch main log
tail -f logs/sls_simulation.log

# Watch telemetry
tail -f logs/telemetry.csv

# Filter for specific subsystem
grep "FCC" logs/sls_simulation.log
```

#### Performance Analysis

Analyze telemetry data:

```bash
# Extract altitude data
awk -F',' '$4=="Vehicle_Altitude" {print $2","$6}' logs/telemetry.csv

# Find maximum values
awk -F',' '$4=="Vehicle_Velocity" {if($6>max) max=$6} END {print max}' logs/telemetry.csv
```

### Troubleshooting

#### Common Issues

1. **Build Failures**
   - Verify QNX environment is set up correctly
   - Check compiler paths and permissions
   - Ensure all dependencies are installed

2. **Runtime Errors**
   - Check available memory and disk space
   - Verify configuration file syntax
   - Review system logs for error messages

3. **Performance Issues**
   - Reduce simulation rate in configuration
   - Disable unnecessary logging
   - Check system resource usage

#### Debug Techniques

1. **Enable Debug Logging**
   ```bash
   export SLS_CONFIG_OVERRIDE="log_level=DEBUG"
   ```

2. **Run with Debugger**
   ```bash
   gdb bin/space_launch_sim
   ```

3. **Memory Analysis**
   ```bash
   valgrind --tool=memcheck bin/space_launch_sim
   ```

### Best Practices

1. **Configuration Management**
   - Use version control for configuration files
   - Document any custom modifications
   - Test configuration changes in isolation

2. **Log Management**
   - Rotate log files regularly
   - Archive telemetry data for analysis
   - Monitor disk space usage

3. **Performance Optimization**
   - Profile critical code paths
   - Optimize real-time loops
   - Monitor system resource usage

### Advanced Features

#### Custom Subsystems

To add a custom subsystem:

1. Create source file in `src/subsystems/`
2. Implement thread function with signature `void* subsystem_thread(void* arg)`
3. Add to subsystem configuration in `sls_config.h`
4. Update Makefile to include new source

#### Fault Injection

Test system robustness with fault injection:

```c
// Inject sensor fault
sls_simulate_sensor_fault(0.01); // 1% probability

// Inject communication failure
sls_ipc_simulate_timeout(SUBSYS_TELEMETRY);
```

#### Hardware Integration

The simulation can be extended to interface with real hardware:

- CAN bus communication
- Serial device interfaces
- GPIO control
- Analog sensor inputs

### Support and Resources

- **Documentation**: See `docs/` directory
- **Examples**: Check `examples/` directory
- **API Reference**: See `docs/api-reference.md`
- **Issue Tracking**: Use project issue tracker
- **Community**: Join project forums or mailing lists
