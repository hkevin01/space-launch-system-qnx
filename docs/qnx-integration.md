# QNX Integration Guide

## Overview

This document provides a comprehensive guide to understanding how QNX Neutrino RTOS integrates with our space launch system simulation, demonstrating the complete system integration from hardware abstraction to application-level services.

## QNX System Stack

### Complete Architecture View

```
┌─────────────────────────────────────────────────────────────────┐
│                   Application Layer                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │ Flight Ctrl │  │Engine Ctrl  │  │ Telemetry   │  │GUI App  │ │
│  │ (Priority   │  │ (Priority   │  │ (Priority   │  │(Priority│ │
│  │    50)      │  │    45)      │  │    40)      │  │   20)   │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                      IPC Layer                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Message Passing    │ Shared Memory  │    Pulses        │   │
│  │  • Commands         │ • Telemetry    │ • Heartbeats     │   │
│  │  • Status Updates   │ • Sensor Data  │ • Interrupts     │   │
│  │  • Configuration    │ • Log Buffers  │ • Timing Sync    │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                   Resource Managers                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │   I/O       │  │   Network   │  │ File System │  │ Serial  │ │
│  │  Manager    │  │  Manager    │  │  Manager    │  │Manager  │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                  QNX Microkernel                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │  Scheduler  │  │   Memory    │  │    IPC      │  │ Timing  │ │
│  │             │  │  Manager    │  │  Manager    │  │Services │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                Hardware Abstraction                            │
│       CPU        │    Memory      │   I/O Devices │  Network   │
└─────────────────────────────────────────────────────────────────┘
```

## System Integration Points

### 1. Process Architecture

Our space launch system leverages QNX's process-based architecture for fault isolation:

```c
// System initialization with QNX process spawning
int initialize_launch_system(void)
{
    process_info_t processes[] = {
        {"flight_control", "/opt/sls/bin/flight_control", 50, SPAWN_DETACHED},
        {"engine_control", "/opt/sls/bin/engine_control", 45, SPAWN_DETACHED},
        {"telemetry", "/opt/sls/bin/telemetry", 40, SPAWN_DETACHED},
        {"navigation", "/opt/sls/bin/navigation", 35, SPAWN_DETACHED},
        {"safety_monitor", "/opt/sls/bin/safety", 60, SPAWN_DETACHED},
        {NULL, NULL, 0, 0}  // Terminator
    };
    
    for (int i = 0; processes[i].name != NULL; i++) {
        pid_t pid = spawn_subsystem(&processes[i]);
        if (pid == -1) {
            sls_log_error("Failed to spawn %s", processes[i].name);
            return -1;
        }
        
        // Register process for monitoring
        register_process_monitor(pid, processes[i].name);
        sls_log_info("Started %s with PID %d, priority %d", 
                     processes[i].name, pid, processes[i].priority);
    }
    
    return 0;
}

// QNX-specific process spawning with inheritance
pid_t spawn_subsystem(const process_info_t *proc_info)
{
    struct inheritance inherit;
    struct _thread_attr attr;
    char *argv[] = {(char*)proc_info->executable, NULL};
    
    // Configure inheritance for real-time scheduling
    inherit.flags = SPAWN_INHERIT_SCHED;
    inherit.policy = SCHED_FIFO;  // First-in-first-out real-time scheduling
    inherit.priority = proc_info->priority;
    
    // Set up thread attributes
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    
    // Spawn process with QNX-specific flags
    return spawn(proc_info->executable, 
                 proc_info->flags, 
                 NULL,              // stdin
                 &inherit,          // inheritance
                 argv,              // arguments
                 environ);          // environment
}
```

### 2. Inter-Process Communication

QNX message passing provides the backbone for subsystem communication:

```c
// Complete IPC message structure
typedef struct {
    struct _pulse pulse;           // QNX pulse header
    uint32_t msg_type;            // Message classification
    uint32_t source_id;           // Source subsystem
    uint32_t dest_id;             // Destination subsystem
    uint64_t timestamp;           // QNX timestamp
    uint32_t sequence_number;     // For message ordering
    uint32_t data_length;         // Payload size
    uint8_t data[SLS_MAX_MSG_SIZE]; // Actual data
    uint32_t checksum;            // Data integrity
} sls_ipc_message_t;

// QNX channel-based communication server
int run_ipc_server(void)
{
    int chid, rcvid;
    sls_ipc_message_t msg;
    struct _msg_info msg_info;
    
    // Create QNX channel for receiving messages
    chid = ChannelCreate(_NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK);
    if (chid == -1) {
        perror("ChannelCreate failed");
        return -1;
    }
    
    // Register channel in global namespace
    if (name_attach(NULL, SLS_IPC_CHANNEL_MAIN, chid) == NULL) {
        perror("name_attach failed");
        ChannelDestroy(chid);
        return -1;
    }
    
    sls_log_info("IPC server started on channel %d", chid);
    
    // Main message processing loop
    while (system_running) {
        // Wait for message with timeout
        rcvid = MsgReceive(chid, &msg, sizeof(msg), &msg_info);
        
        if (rcvid == -1) {
            if (errno == EINTR) continue;  // Interrupted by signal
            perror("MsgReceive failed");
            break;
        }
        
        // Handle pulse messages (asynchronous notifications)
        if (rcvid == 0) {
            handle_pulse_message(&msg.pulse);
            continue;
        }
        
        // Process regular messages
        int result = process_ipc_message(&msg, &msg_info);
        
        // Send reply to unblock sender
        if (MsgReply(rcvid, result, NULL, 0) == -1) {
            perror("MsgReply failed");
        }
    }
    
    // Cleanup
    name_detach(NULL, SLS_IPC_CHANNEL_MAIN);
    ChannelDestroy(chid);
    return 0;
}

// Client-side message sending with error handling
int send_telemetry_data(const telemetry_point_t *data)
{
    int coid;
    sls_ipc_message_t msg;
    int reply;
    
    // Connect to telemetry server
    coid = name_open(SLS_IPC_CHANNEL_TELEMETRY, 0);
    if (coid == -1) {
        sls_log_error("Failed to connect to telemetry channel");
        return -1;
    }
    
    // Prepare message
    memset(&msg, 0, sizeof(msg));
    msg.msg_type = MSG_TYPE_TELEMETRY;
    msg.source_id = get_subsystem_id();
    msg.timestamp = ClockTime(CLOCK_REALTIME, NULL);
    msg.data_length = sizeof(telemetry_point_t);
    memcpy(msg.data, data, sizeof(telemetry_point_t));
    msg.checksum = calculate_checksum(&msg);
    
    // Send message and wait for reply
    if (MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
        perror("MsgSend failed");
        name_close(coid);
        return -1;
    }
    
    name_close(coid);
    return reply;
}
```

### 3. Shared Memory Integration

High-performance data sharing using QNX shared memory:

```c
// Shared memory telemetry buffer for high-frequency data
typedef struct {
    volatile uint64_t write_index;    // Writer position
    volatile uint64_t read_index;     // Reader position
    uint32_t buffer_size;             // Total buffer size
    uint32_t entry_size;              // Size of each entry
    pthread_mutex_t write_mutex;      // Writer synchronization
    sem_t data_available;             // Signal new data
    telemetry_entry_t entries[SLS_TELEMETRY_BUFFER_SIZE];
} telemetry_shm_buffer_t;

// Initialize shared memory for telemetry
int init_telemetry_shm(void)
{
    int shm_fd;
    telemetry_shm_buffer_t *shm_buffer;
    
    // Create or open shared memory object
    shm_fd = shm_open(SLS_TELEMETRY_SHM_NAME, 
                      O_CREAT | O_RDWR, 
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (shm_fd == -1) {
        perror("shm_open failed");
        return -1;
    }
    
    // Set size of shared memory
    if (ftruncate(shm_fd, sizeof(telemetry_shm_buffer_t)) == -1) {
        perror("ftruncate failed");
        close(shm_fd);
        return -1;
    }
    
    // Map shared memory into process address space
    shm_buffer = mmap(NULL, 
                      sizeof(telemetry_shm_buffer_t),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      shm_fd,
                      0);
    if (shm_buffer == MAP_FAILED) {
        perror("mmap failed");
        close(shm_fd);
        return -1;
    }
    
    // Initialize buffer structure (first process only)
    if (shm_buffer->buffer_size == 0) {
        shm_buffer->write_index = 0;
        shm_buffer->read_index = 0;
        shm_buffer->buffer_size = SLS_TELEMETRY_BUFFER_SIZE;
        shm_buffer->entry_size = sizeof(telemetry_entry_t);
        
        // Initialize synchronization objects
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shm_buffer->write_mutex, &mutex_attr);
        
        sem_init(&shm_buffer->data_available, 1, 0);  // Shared semaphore
    }
    
    // Store global reference
    g_telemetry_shm = shm_buffer;
    close(shm_fd);  // File descriptor no longer needed
    
    return 0;
}

// High-performance telemetry data writing
int write_telemetry_shm(const telemetry_entry_t *entry)
{
    telemetry_shm_buffer_t *buffer = g_telemetry_shm;
    uint64_t next_write;
    
    if (!buffer) return -1;
    
    // Lock writer mutex
    if (pthread_mutex_lock(&buffer->write_mutex) != 0) {
        return -1;
    }
    
    // Calculate next write position
    next_write = (buffer->write_index + 1) % buffer->buffer_size;
    
    // Check for buffer overflow
    if (next_write == buffer->read_index) {
        // Buffer full - could drop oldest data or block
        sls_log_warning("Telemetry buffer overflow, dropping data");
        pthread_mutex_unlock(&buffer->write_mutex);
        return -1;
    }
    
    // Copy data to buffer
    memcpy(&buffer->entries[buffer->write_index], entry, sizeof(telemetry_entry_t));
    
    // Update write index (atomic on most architectures)
    buffer->write_index = next_write;
    
    pthread_mutex_unlock(&buffer->write_mutex);
    
    // Signal that new data is available
    sem_post(&buffer->data_available);
    
    return 0;
}
```

### 4. Real-Time Timer Integration

Precise timing using QNX timer services:

```c
// High-precision timing for control loops
typedef struct {
    timer_t timer_id;
    struct sigevent timer_event;
    struct itimerspec timer_spec;
    int channel_id;
    uint32_t pulse_code;
    uint64_t cycle_count;
    uint64_t missed_deadlines;
} rt_timer_t;

// Initialize real-time timer for flight control loop
int init_flight_control_timer(rt_timer_t *timer, uint32_t frequency_hz)
{
    int pulse_id;
    
    // Create channel for timer pulses
    timer->channel_id = ChannelCreate(0);
    if (timer->channel_id == -1) {
        perror("ChannelCreate for timer failed");
        return -1;
    }
    
    // Create pulse for timer notifications
    pulse_id = pulse_attach(timer->channel_id, MSG_FLAG_ALLOC_PULSE, 0, NULL, NULL);
    if (pulse_id == -1) {
        perror("pulse_attach failed");
        ChannelDestroy(timer->channel_id);
        return -1;
    }
    timer->pulse_code = pulse_id;
    
    // Set up timer event structure
    timer->timer_event.sigev_notify = SIGEV_PULSE;
    timer->timer_event.sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0, 
                                                  timer->channel_id, 
                                                  _NTO_SIDE_CHANNEL, 0);
    if (timer->timer_event.sigev_coid == -1) {
        perror("ConnectAttach failed");
        ChannelDestroy(timer->channel_id);
        return -1;
    }
    
    timer->timer_event.sigev_priority = getprio(0);
    timer->timer_event.sigev_code = timer->pulse_code;
    
    // Create the timer
    if (timer_create(CLOCK_REALTIME, &timer->timer_event, &timer->timer_id) == -1) {
        perror("timer_create failed");
        ConnectDetach(timer->timer_event.sigev_coid);
        ChannelDestroy(timer->channel_id);
        return -1;
    }
    
    // Configure timer for periodic operation
    uint64_t period_ns = 1000000000ULL / frequency_hz;  // Convert Hz to nanoseconds
    timer->timer_spec.it_value.tv_sec = 0;
    timer->timer_spec.it_value.tv_nsec = period_ns;
    timer->timer_spec.it_interval.tv_sec = 0;
    timer->timer_spec.it_interval.tv_nsec = period_ns;
    
    // Start the timer
    if (timer_settime(timer->timer_id, 0, &timer->timer_spec, NULL) == -1) {
        perror("timer_settime failed");
        timer_delete(timer->timer_id);
        ConnectDetach(timer->timer_event.sigev_coid);
        ChannelDestroy(timer->channel_id);
        return -1;
    }
    
    timer->cycle_count = 0;
    timer->missed_deadlines = 0;
    
    sls_log_info("Flight control timer initialized: %u Hz", frequency_hz);
    return 0;
}

// Real-time control loop with deadline monitoring
void flight_control_loop(rt_timer_t *timer)
{
    struct _pulse pulse;
    uint64_t start_time, end_time, deadline;
    const uint64_t max_execution_time = 15000000;  // 15ms deadline
    
    sls_log_info("Starting flight control loop");
    
    while (system_running) {
        // Wait for timer pulse
        if (MsgReceive(timer->channel_id, &pulse, sizeof(pulse), NULL) == -1) {
            if (errno == EINTR) continue;
            perror("MsgReceive timer pulse failed");
            break;
        }
        
        // Verify this is our timer pulse
        if (pulse.code != timer->pulse_code) {
            continue;
        }
        
        // Record start time for deadline monitoring
        start_time = ClockTime(CLOCK_REALTIME, NULL);
        deadline = start_time + max_execution_time;
        
        // Execute flight control algorithms
        flight_control_cycle();
        
        // Check if we met our deadline
        end_time = ClockTime(CLOCK_REALTIME, NULL);
        if (end_time > deadline) {
            timer->missed_deadlines++;
            sls_log_warning("Flight control deadline missed by %llu ns", 
                           end_time - deadline);
        }
        
        timer->cycle_count++;
        
        // Log performance statistics periodically
        if (timer->cycle_count % 1000 == 0) {
            double miss_rate = (double)timer->missed_deadlines / timer->cycle_count;
            sls_log_info("Flight control stats: %llu cycles, %.2f%% deadline misses",
                        timer->cycle_count, miss_rate * 100.0);
        }
    }
}
```

### 5. Fault Detection and Recovery

QNX process monitoring and automatic recovery:

```c
// Process monitoring and automatic restart
typedef struct {
    pid_t pid;
    char name[64];
    char executable[256];
    int priority;
    uint32_t restart_count;
    uint64_t last_restart_time;
    bool critical;  // Critical processes get immediate restart
} monitored_process_t;

static monitored_process_t monitored_processes[MAX_MONITORED_PROCESSES];
static int process_count = 0;

// QNX-specific process monitoring thread
void* process_monitor_thread(void *arg)
{
    struct sigevent event;
    int death_chid, death_coid;
    
    // Create channel for death notifications
    death_chid = ChannelCreate(0);
    if (death_chid == -1) {
        sls_log_error("Failed to create death notification channel");
        return NULL;
    }
    
    death_coid = ConnectAttach(ND_LOCAL_NODE, 0, death_chid, _NTO_SIDE_CHANNEL, 0);
    if (death_coid == -1) {
        sls_log_error("Failed to attach to death notification channel");
        ChannelDestroy(death_chid);
        return NULL;
    }
    
    // Set up death notification event
    event.sigev_notify = SIGEV_PULSE;
    event.sigev_coid = death_coid;
    event.sigev_priority = getprio(0);
    event.sigev_code = PULSE_CODE_PROCESS_DEATH;
    
    while (system_running) {
        // Check all monitored processes
        for (int i = 0; i < process_count; i++) {
            monitored_process_t *proc = &monitored_processes[i];
            
            // Check if process is still alive
            if (kill(proc->pid, 0) == -1 && errno == ESRCH) {
                sls_log_error("Process %s (PID %d) has died", proc->name, proc->pid);
                
                // Determine restart strategy
                if (should_restart_process(proc)) {
                    restart_process(proc);
                } else {
                    sls_log_error("Process %s exceeded restart limit, initiating safe shutdown",
                                 proc->name);
                    initiate_safe_shutdown();
                    break;
                }
            }
        }
        
        // Sleep for monitoring interval
        delay(1000);  // QNX delay function (1 second)
    }
    
    ConnectDetach(death_coid);
    ChannelDestroy(death_chid);
    return NULL;
}

// Intelligent process restart with backoff
int restart_process(monitored_process_t *proc)
{
    uint64_t current_time = ClockTime(CLOCK_REALTIME, NULL);
    uint64_t time_since_last_restart = current_time - proc->last_restart_time;
    
    // Implement exponential backoff for non-critical processes
    if (!proc->critical && proc->restart_count > 0) {
        uint64_t min_interval = 1000000000ULL * (1 << (proc->restart_count - 1));  // Exponential backoff
        if (time_since_last_restart < min_interval) {
            sls_log_info("Delaying restart of %s (backoff period)", proc->name);
            return -1;
        }
    }
    
    // Attempt restart
    pid_t new_pid = spawn_subsystem_by_name(proc->name);
    if (new_pid == -1) {
        sls_log_error("Failed to restart process %s", proc->name);
        return -1;
    }
    
    // Update process tracking
    proc->pid = new_pid;
    proc->restart_count++;
    proc->last_restart_time = current_time;
    
    sls_log_info("Successfully restarted %s with new PID %d (restart #%u)",
                proc->name, new_pid, proc->restart_count);
    
    return 0;
}
```

## Performance Monitoring

### QNX-Specific Metrics

```c
// Comprehensive system performance monitoring
typedef struct {
    uint64_t ipc_messages_sent;
    uint64_t ipc_messages_received;
    uint64_t ipc_total_latency_ns;
    uint64_t shared_memory_reads;
    uint64_t shared_memory_writes;
    uint32_t process_restarts;
    uint32_t missed_deadlines;
    double cpu_utilization;
    double memory_utilization;
} system_performance_t;

// Real-time performance monitoring
void monitor_system_performance(void)
{
    system_performance_t perf;
    procfs_info info;
    uint64_t total_memory, free_memory;
    
    // Get process information from QNX procfs
    if (devctl(DCMD_PROC_INFO, &info, sizeof(info), NULL) == EOK) {
        perf.cpu_utilization = (double)info.cpu_time / info.total_time * 100.0;
    }
    
    // Get memory information
    if (devctl(DCMD_PROC_PAGEDATA, &total_memory, sizeof(total_memory), NULL) == EOK) {
        perf.memory_utilization = (double)(total_memory - free_memory) / total_memory * 100.0;
    }
    
    // Log performance metrics
    sls_log_info("Performance: CPU %.1f%%, Memory %.1f%%, IPC Latency %llu ns",
                perf.cpu_utilization, perf.memory_utilization, 
                perf.ipc_total_latency_ns / perf.ipc_messages_sent);
}
```

## Conclusion

This integration demonstrates how QNX Neutrino's microkernel architecture, real-time scheduling, and robust IPC mechanisms provide the foundation for a safety-critical space launch system. The modular design ensures fault isolation, while the deterministic real-time behavior meets the stringent timing requirements of aerospace applications.

Key integration benefits:
- **Process isolation** prevents cascade failures
- **Real-time scheduling** ensures deterministic response times  
- **Efficient IPC** enables high-performance subsystem communication
- **Built-in fault tolerance** provides automatic recovery capabilities
- **Scalable architecture** supports system growth and modification

The complete system showcases QNX's capabilities for mission-critical applications where failure is not an option.
