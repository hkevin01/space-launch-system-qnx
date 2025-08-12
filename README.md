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
## Space Launch System Simulation — QNX Neutrino RTOS Demo

This repository is now QNX-only and showcases core QNX Neutrino RTOS features working together in a simplified space launch system simulation.

What you'll see running on QNX:
- Native QNX message passing between an Operator Console and the Flight Control Computer (FCC) service
- QNX pulses/timers driving periodic simulation ticks
- QNX slog2 structured logging categorized by component
- A minimal QNX resource manager exporting telemetry at /dev/sls_telemetry
- Thread priorities suitable for a real-time system

### QNX features demonstrated
- Message passing: ChannelCreate/MsgReceive/MsgSend/MsgReply via name_attach/name_open helpers
- Pulses and timers: SIGEV_PULSE with timer_create/timer_settime
- Scheduling: SCHED_RR priorities for key threads
- Resource manager: resmgr/iofunc device at /dev/sls_telemetry
- Logging: slog2 buffer set "SLS" with INFO/WARN/ERROR macros

## Build (QNX SDP 7.1+)

Prerequisites: qcc, make on a QNX development host or a QNX target system.

```sh
make all
```

Outputs:
- build/sls_qnx      — main simulation and FCC service
- build/sls_console  — terminal operator console

## Run

```sh
./scripts/qnx_run.sh
```

This will start the simulation and then launch the operator console.

Operator console commands:
- status
- go
- nogo
- abort
- throttle N   (N = 0..100)
- quit

Telemetry stream:
```sh
cat /dev/sls_telemetry
```
Example line: `1691000000.123,alt=12.34,vel=3.21,thr=70,go=1`

Logs (on target):
```sh
slog2info -l | grep SLS
```

## Repo layout

```
src/
	qnx/           # QNX IPC, pulses, resource manager, main
	common/        # slog2 wrapper
	ui/            # Operator console (text UI)
scripts/
	qnx_build.sh   # Build helper
	qnx_run.sh     # Run helper (starts sim + console)
Makefile         # QNX-only build (qcc)
```

## Removed components (for maintainers)

The following non-QNX artifacts are deprecated and should be deleted if present:
- gui/ (PyQt6 GUI and tests)
- podman/ and docker/* (containerization, noVNC)
- .github/workflows/* that assume Ubuntu/Linux GUI testing
- POSIX mock shims and Python requirements not needed on QNX

## License

MIT License. See LICENSE.
