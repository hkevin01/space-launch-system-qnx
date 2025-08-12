# QNX Space Launch System Simulation

A real-time space launch system simulation demonstrating core QNX Neutrino RTOS features including native message passing, pulses/timers, resource managers, and slog2 logging.

## Overview

This QNX-only project showcases a simplified Flight Control Computer (FCC) service that communicates with an operator console via QNX message passing, generates telemetry through a resource manager, and logs system events with slog2.

### QNX Features Demonstrated

- **Message Passing**: name_attach/name_open with MsgSend/MsgReceive/MsgReply
- **Pulses & Timers**: SIGEV_PULSE periodic simulation ticks
- **Resource Manager**: /dev/sls_telemetry device with resmgr/iofunc
- **Structured Logging**: slog2 buffer set "SLS" with component categorization
- **Real-time Scheduling**: SCHED_RR thread priorities

## Project Structure

```
src/
├── qnx/                   # QNX-specific implementations
│   ├── main_qnx.c        # Main simulation + FCC service
│   ├── ipc.{h,c}         # Message passing & pulse abstractions
│   └── rmgr_telemetry.{h,c} # Telemetry resource manager
├── common/
│   └── slog.{h,c}        # slog2 wrapper with component macros
└── ui/
    └── console.c         # Text-based operator console
scripts/
├── qnx_build.sh          # Build helper script
└── qnx_run.sh            # Run helper (starts sim + console)
```

## Build & Run

### Prerequisites

- QNX SDP 7.1+ with qcc compiler
- make utility

### Build

```bash
make all
```

**Outputs:**

- `build/sls_qnx` — main simulation and FCC service
- `build/sls_console` — terminal operator console

### Run

```bash
./scripts/qnx_run.sh
```

This starts the simulation and then launches the operator console.

**Operator console commands:**

- `status` — show current system state
- `go` — approve mission proceed
- `nogo` — mission hold
- `abort` — emergency abort
- `throttle N` — set throttle (N = 0..100)
- `quit` — exit console

**Telemetry stream:**

```bash
cat /dev/sls_telemetry
```

Example line: `1691000000.123,alt=12.34,vel=3.21,thr=70,go=1`

**System logs:**

```bash
slog2info -l | grep SLS
```

## Repository Layout

```text
src/
    qnx/           # QNX IPC, pulses, resource manager, main
    common/        # slog2 wrapper
    ui/            # Operator console (text UI)
scripts/
    qnx_build.sh   # Build helper
    qnx_run.sh     # Run helper (starts sim + console)
Makefile           # QNX-only build (qcc)
```

## Removed Components

The following non-QNX artifacts have been removed:

- `gui/` (PyQt6 GUI and tests)
- `podman/` and `docker/` (containerization, noVNC)
- `.github/workflows/` (Linux GUI CI)
- POSIX mock shims not needed on QNX

## License

MIT License
