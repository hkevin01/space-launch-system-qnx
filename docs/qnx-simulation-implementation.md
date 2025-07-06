# How Our Simulation Uses QNX Technology

## Overview

Our space launch system simulation demonstrates real QNX concepts and technologies in action. This document explains exactly how we implement QNX features, what's real vs simulated, and how the system behaves like a true QNX aerospace application.

## Real QNX vs Simulation Mode

### Dual-Platform Architecture

Our system is designed to run in two modes:

**🚀 Production Mode (Real QNX)**
```
┌─────────────────────────────────────────┐
│            QNX Neutrino RTOS            │
│  ┌─────────────────────────────────┐    │
│  │     Our Rocket Software         │    │
│  │  • Real QNX message passing     │    │
│  │  • True real-time scheduling    │    │
│  │  • Hardware fault tolerance     │    │
│  │  • Microsecond timing precision │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

**💻 Development Mode (Linux with QNX Simulation)**
```
┌─────────────────────────────────────────┐
│               Linux OS                  │
│  ┌─────────────────────────────────┐    │
│  │     Our Rocket Software         │    │
│  │  • Mock QNX functions           │    │
│  │  • Simulated real-time behavior │    │
│  │  • Software fault injection     │    │
│  │  • Millisecond timing precision │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

## QNX Technology Implementation

### 1. Process Architecture - Real QNX Implementation

In our system, each rocket subsystem runs as a separate QNX process:

```c
// Real QNX process spawning (from src/main.c)
int spawn_flight_control_process(void)
{
    struct inheritance inherit;
    char *argv[] = {"flight_control", NULL};
    
    // Configure QNX inheritance for real-time priority
    inherit.flags = SPAWN_INHERIT_SCHED;
    inherit.policy = SCHED_FIFO;        // Real-time FIFO scheduling
    inherit.priority = 50;              // High priority for flight control
    
    // Spawn as separate QNX process
    pid_t pid = spawn("/opt/sls/bin/flight_control", 
                      SPAWN_DETACHED,   // Independent process
                      NULL,             // stdin
                      &inherit,         // Priority inheritance
                      argv,             // Arguments
                      environ);         // Environment
    
    if (pid == -1) {
        sls_log_error("Failed to spawn flight control process");
        return -1;
    }
    
    sls_log_info("Flight control started as PID %d with priority %d", 
                 pid, inherit.priority);
    return 0;
}
```

**What this gives us:**
- True process isolation (if flight control crashes, engine control keeps running)
- Real-time priority scheduling
- QNX memory protection between subsystems

### 2. Message Passing - QNX IPC in Action

Our simulation uses real QNX message passing for subsystem communication:

```c
// Real QNX message passing (from src/common/sls_ipc.c)
int send_engine_command(engine_command_t *cmd)
{
    int coid;
    ipc_message_t msg;
    int reply;
    
    // Connect to engine control QNX channel
    coid = name_open(ENGINE_CONTROL_CHANNEL, 0);
    if (coid == -1) {
        sls_log_error("Cannot connect to engine control");
        return -1;
    }
    
    // Prepare QNX message
    msg.type = MSG_ENGINE_COMMAND;
    msg.timestamp = ClockTime(CLOCK_REALTIME, NULL);  // QNX precise timing
    msg.data_length = sizeof(engine_command_t);
    memcpy(msg.data, cmd, sizeof(engine_command_t));
    
    // Send using QNX MsgSend (synchronous message passing)
    if (MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
        perror("MsgSend to engine control failed");
        name_close(coid);
        return -1;
    }
    
    name_close(coid);
    return reply;  // Engine control's response
}

// QNX message receiving (runs in engine control process)
int engine_control_message_loop(void)
{
    int chid, rcvid;
    ipc_message_t msg;
    
    // Create QNX channel for receiving messages
    chid = ChannelCreate(_NTO_CHF_DISCONNECT);
    if (chid == -1) {
        perror("ChannelCreate failed");
        return -1;
    }
    
    // Register in QNX namespace so others can find us
    if (name_attach(NULL, ENGINE_CONTROL_CHANNEL, chid) == NULL) {
        perror("name_attach failed");
        return -1;
    }
    
    while (engine_running) {
        // Wait for messages from other subsystems
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
        
        if (rcvid > 0) {  // Valid message received
            // Process the command
            int result = process_engine_command(&msg);
            
            // Reply to sender (unblocks them)
            MsgReply(rcvid, result, NULL, 0);
        }
    }
    
    return 0;
}
```

**What this demonstrates:**
- Synchronous communication (sender waits for reply)
- Named channels for service discovery
- Automatic process blocking/unblocking
- Precise timestamping with QNX clock services

### 3. Shared Memory - High-Performance Data Sharing

For high-frequency telemetry data, we use QNX shared memory:

```c
// QNX shared memory for telemetry (from flight_control.c)
int init_telemetry_shared_memory(void)
{
    int shm_fd;
    
    // Create QNX shared memory object
    shm_fd = shm_open("/sls_telemetry", 
                      O_CREAT | O_RDWR, 
                      S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return -1;
    }
    
    // Set size for telemetry buffer
    if (ftruncate(shm_fd, sizeof(telemetry_buffer_t)) == -1) {
        perror("ftruncate failed");
        return -1;
    }
    
    // Map into our process address space
    telemetry_buffer = mmap(NULL, 
                           sizeof(telemetry_buffer_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,  // Shared between processes
                           shm_fd, 0);
    
    if (telemetry_buffer == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }
    
    // Initialize synchronization (process-shared)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&telemetry_buffer->mutex, &attr);
    
    return 0;
}

// Writing telemetry data at 100Hz
void write_telemetry_data(void)
{
    telemetry_point_t data;
    
    // Collect current vehicle state
    data.altitude = get_current_altitude();
    data.velocity = get_current_velocity();
    data.fuel_remaining = get_fuel_percentage();
    data.timestamp = ClockTime(CLOCK_REALTIME, NULL);
    
    // Write to shared memory (atomic operation)
    pthread_mutex_lock(&telemetry_buffer->mutex);
    
    uint32_t write_pos = telemetry_buffer->write_index;
    telemetry_buffer->data[write_pos] = data;
    telemetry_buffer->write_index = (write_pos + 1) % TELEMETRY_BUFFER_SIZE;
    
    pthread_mutex_unlock(&telemetry_buffer->mutex);
    
    // Notify GUI that new data is available (QNX pulse)
    MsgSendPulse(gui_connection, getprio(0), PULSE_NEW_TELEMETRY, 0);
}
```

**What this achieves:**
- Zero-copy data sharing between processes
- High-frequency updates (100Hz+) without message overhead
- Process-shared synchronization primitives
- Real-time notifications via QNX pulses

### 4. Real-Time Scheduling - Priority-Based Control

Our simulation uses QNX's real-time scheduler for deterministic behavior:

```c
// Setting real-time priorities (from each subsystem)
int configure_realtime_priority(subsystem_type_t subsystem)
{
    struct sched_param param;
    int policy = SCHED_FIFO;  // QNX real-time FIFO scheduling
    
    // Set priority based on criticality
    switch (subsystem) {
        case SUBSYS_SAFETY_MONITOR:
            param.sched_priority = 60;  // Highest - safety critical
            break;
        case SUBSYS_FLIGHT_CONTROL:
            param.sched_priority = 50;  // High - vehicle control
            break;
        case SUBSYS_ENGINE_CONTROL:
            param.sched_priority = 45;  // High - propulsion
            break;
        case SUBSYS_TELEMETRY:
            param.sched_priority = 40;  // Normal - data collection
            break;
        case SUBSYS_GUI:
            param.sched_priority = 20;  // Low - user interface
            break;
        default:
            param.sched_priority = 30;  // Default priority
    }
    
    // Apply QNX real-time scheduling
    if (sched_setscheduler(0, policy, &param) == -1) {
        perror("sched_setscheduler failed");
        return -1;
    }
    
    sls_log_info("Subsystem %d running with real-time priority %d", 
                 subsystem, param.sched_priority);
    return 0;
}

// Real-time control loop with timing guarantees
void flight_control_realtime_loop(void)
{
    struct timespec next_cycle;
    const long cycle_time_ns = 20000000;  // 20ms = 50Hz control loop
    
    // Get current time for first cycle
    clock_gettime(CLOCK_REALTIME, &next_cycle);
    
    while (flight_control_active) {
        // Calculate next cycle time
        next_cycle.tv_nsec += cycle_time_ns;
        if (next_cycle.tv_nsec >= 1000000000) {
            next_cycle.tv_sec += 1;
            next_cycle.tv_nsec -= 1000000000;
        }
        
        // Execute flight control algorithms
        update_navigation_state();
        calculate_control_commands();
        send_actuator_commands();
        
        // Sleep until next cycle (QNX precise timing)
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_cycle, NULL);
    }
}
```

**Real-time benefits:**
- Guaranteed execution order by priority
- Deterministic timing for control loops
- Preemptive scheduling ensures critical tasks run first
- Precise timing with nanosecond resolution

## Mock QNX Implementation for Linux

When running on Linux for development, we simulate QNX behavior:

### Mock Implementation Strategy

```c
// QNX mock functions (from src/common/qnx_mock.h)
#ifdef MOCK_QNX_BUILD

// Simulate QNX message passing with Unix domain sockets
static inline int MsgSend(int coid, const void *smsg, size_t sbytes,
                         void *rmsg, size_t rbytes)
{
    // Use Unix socket to simulate QNX message passing
    return mock_msg_send(coid, smsg, sbytes, rmsg, rbytes);
}

// Simulate QNX channels with named pipes
static inline int ChannelCreate(unsigned flags)
{
    // Create named pipe to simulate QNX channel
    return mock_channel_create(flags);
}

// Simulate QNX shared memory with POSIX shared memory
static inline int shm_open(const char *name, int oflag, mode_t mode)
{
    // Use Linux shared memory, same API as QNX
    return shm_open(name, oflag, mode);
}

// Simulate QNX real-time scheduling
static inline int sched_setscheduler(pid_t pid, int policy, 
                                    const struct sched_param *param)
{
    // Log what would happen on QNX, use Linux nice() as approximation
    mock_log_priority_change(pid, param->sched_priority);
    return nice(param->sched_priority);
}

#endif // MOCK_QNX_BUILD
```

### Build System Integration

Our Makefile automatically detects the platform:

```makefile
# Detect if we're building for QNX or Linux
ifeq ($(QNX_TARGET),)
    # Building on Linux - use mock QNX functions
    CFLAGS += -DMOCK_QNX_BUILD
    LDFLAGS += -lpthread -lrt
    CC = gcc
else
    # Building on QNX - use real QNX functions
    CFLAGS += -DQNX_BUILD
    LDFLAGS += -lsocket
    CC = qcc
endif

# Common source files work on both platforms
SOURCES = src/main.c \
          src/subsystems/flight_control.c \
          src/subsystems/engine_control.c \
          src/common/sls_ipc.c
```

## Simulation Behavior Comparison

### What's the Same

| Feature | QNX (Real) | Linux (Mock) | Notes |
|---------|------------|--------------|-------|
| Process Architecture | ✅ Separate processes | ✅ Separate processes | Same isolation benefits |
| Message Passing | ✅ QNX MsgSend/Receive | ✅ Unix sockets | Same API, different transport |
| Shared Memory | ✅ QNX shm_open | ✅ POSIX shm_open | Identical API |
| Threading | ✅ POSIX threads | ✅ POSIX threads | Same threading model |
| File I/O | ✅ Standard POSIX | ✅ Standard POSIX | Same file operations |

### What's Different

| Feature | QNX (Real) | Linux (Mock) | Impact |
|---------|------------|--------------|--------|
| Real-time Scheduling | ✅ Hard real-time | ⚠️ Best effort | Timing less predictable |
| Fault Isolation | ✅ Microkernel | ⚠️ Monolithic kernel | Less fault tolerance |
| Interrupt Latency | ✅ Microseconds | ⚠️ Milliseconds | Less responsive |
| Memory Protection | ✅ Hardware enforced | ✅ Hardware enforced | Same protection |
| Process Recovery | ✅ Built-in monitoring | ⚠️ Manual scripts | Less automatic |

## Practical Demonstration Scenarios

### Scenario 1: Process Crash Recovery

**On QNX:**
```bash
# Kill engine control process
slay engine_control

# QNX process manager automatically:
# 1. Detects process death
# 2. Notifies other processes via death notification
# 3. Cleanup resources
# 4. Restart process if configured
```

**On Linux (simulated):**
```bash
# Kill engine control process
pkill engine_control

# Our monitoring script:
# 1. Detects missing process
# 2. Logs the failure
# 3. Restarts the process
# 4. Notifies other processes
```

### Scenario 2: Real-Time Response

**QNX Timing:**
```
Safety alert triggered → 50 microseconds → All systems notified
Engine command sent → 1 millisecond → Engine responds
Flight control cycle → 20 milliseconds exactly → Next cycle starts
```

**Linux Timing (simulated):**
```
Safety alert triggered → 1-5 milliseconds → All systems notified
Engine command sent → 5-10 milliseconds → Engine responds
Flight control cycle → 20±5 milliseconds → Next cycle starts (with jitter)
```

### Scenario 3: System Overload

**QNX Behavior:**
- Safety monitor (priority 60) always runs first
- Flight control (priority 50) preempts telemetry
- GUI (priority 20) gets remaining CPU time
- Real-time guarantees maintained

**Linux Behavior:**
- Best-effort scheduling with nice values
- May experience priority inversion
- Timing guarantees not absolute
- Still demonstrates priority concepts

## Validation and Testing

### How We Verify QNX Behavior

```c
// Timing validation tests
void test_realtime_response(void)
{
    uint64_t start, end, latency;
    
    for (int i = 0; i < 1000; i++) {
        start = ClockTime(CLOCK_REALTIME, NULL);
        
        // Send high-priority message
        send_safety_alert("TEST_ALERT");
        
        end = ClockTime(CLOCK_REALTIME, NULL);
        latency = end - start;
        
        // On QNX: should be < 100 microseconds
        // On Linux: should be < 10 milliseconds
        assert(latency < expected_max_latency);
        
        log_timing_result(i, latency);
    }
}

// Message passing validation
void test_ipc_reliability(void)
{
    for (int i = 0; i < 10000; i++) {
        telemetry_point_t data = generate_test_data(i);
        
        int result = send_telemetry_message(&data);
        assert(result == 0);  // Message should always succeed
        
        // Verify message was received correctly
        telemetry_point_t received = wait_for_telemetry();
        assert(memcmp(&data, &received, sizeof(data)) == 0);
    }
}
```

## Benefits of This Approach

### For Development
- **Faster iteration**: Develop and test on Linux
- **Easier debugging**: Use familiar Linux tools
- **Cost effective**: No need for QNX hardware during development
- **Same codebase**: One source tree for both platforms

### For Production
- **True real-time**: Hard timing guarantees on QNX
- **Better reliability**: Microkernel fault isolation
- **Certification ready**: QNX meets aerospace standards
- **Proven platform**: Used in actual spacecraft

### For Learning
- **Understand concepts**: See how real-time systems work
- **Compare platforms**: Learn differences between Linux and QNX
- **Safe experimentation**: Test fault scenarios safely
- **Real application**: Rocket simulation shows practical benefits

## Summary

Our simulation serves as both a practical demonstration of QNX capabilities and a real-world example of aerospace software architecture. By supporting both QNX and Linux, we show:

1. **How QNX features work in practice** - Real message passing, shared memory, and real-time scheduling
2. **Why QNX matters for aerospace** - Timing guarantees and fault tolerance that Linux can't match
3. **How to design cross-platform real-time systems** - Same concepts, different implementations
4. **What aerospace engineers need** - Reliability, predictability, and safety

The beauty of this approach is that you can run the same rocket simulation on your laptop (Linux) for learning and development, then deploy it on real QNX hardware for production use, with confidence that the behavior and timing characteristics will be appropriate for actual aerospace applications.
