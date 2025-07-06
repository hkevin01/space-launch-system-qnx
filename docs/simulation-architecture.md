# Space Launch System Simulation Architecture

## Overview

Our QNX-based space launch system simulation provides a high-fidelity model of real rocket systems, implementing the same architectural patterns used in actual spacecraft. This document details the technical architecture and design decisions.

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Mission Control GUI                        │
│                    (PyQt6 Interface)                          │
└─────────────────┬───────────────────────────────────────────────┘
                  │ TCP/IPC Communication
┌─────────────────▼───────────────────────────────────────────────┐
│                    Main Simulation                            │
│                   (QNX Application)                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │   Flight    │  │   Engine    │  │ Telemetry & │  │ Ground  │ │
│  │  Control    │  │  Control    │  │    Comm     │  │ Support │ │
│  │             │  │             │  │             │  │         │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│              QNX IPC (Message Passing & Shared Memory)         │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │ Sensor      │  │ Navigation  │  │ Power       │  │ Thermal │ │
│  │ Processing  │  │ System      │  │ Management  │  │ Control │ │
│  │             │  │             │  │             │  │         │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                   QNX Neutrino Microkernel                     │
└─────────────────────────────────────────────────────────────────┘
```

### Process Architecture

Each major subsystem runs as an independent QNX process:

```c
// Process spawning during initialization
typedef struct {
    const char *name;
    const char *executable;
    int priority;
    uint32_t flags;
} subsystem_config_t;

static const subsystem_config_t subsystems[] = {
    {"flight_control", "fcs_main", 200, SPAWN_DETACH},
    {"engine_control", "ecs_main", 180, SPAWN_DETACH}, 
    {"telemetry", "telemetry_main", 120, SPAWN_DETACH},
    {"navigation", "nav_main", 150, SPAWN_DETACH},
    {"sensor_hub", "sensor_main", 170, SPAWN_DETACH},
    {NULL, NULL, 0, 0}  // Terminator
};

// Spawn all subsystems
for (int i = 0; subsystems[i].name != NULL; i++) {
    pid_t pid = spawn_subsystem(&subsystems[i]);
    if (pid > 0) {
        register_subsystem(subsystems[i].name, pid);
    }
}
```

## Data Flow Architecture

### Message Flow Diagram

```
Ground Control ──────┐
                     │
                     ▼
┌─────────────────────────────────┐     ┌─────────────────┐
│        Mission Sequencer        │────▶│   Safety        │
│     (Main Controller)           │     │   Monitor       │
└─────────────┬───────────────────┘     └─────────────────┘
              │
              ├─────────────────────────────────────────────┐
              │                                             │
              ▼                                             ▼
┌─────────────────────┐                        ┌─────────────────────┐
│   Flight Control    │◀──────────────────────▶│   Engine Control    │
│                     │    Vehicle State       │                     │
│  • Navigation       │    Thrust Commands     │  • Ignition         │
│  • Guidance         │                        │  • Throttle         │
│  • Attitude Control │                        │  • Shutdown         │
└─────────────────────┘                        └─────────────────────┘
              │                                             │
              │                                             │
              ▼                                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Telemetry & Data Logger                         │
│                                                                     │
│  • Sensor Data Collection    • State Logging    • Ground Comm      │
└─────────────────────────────────────────────────────────────────────┘
```

### Inter-Process Communication

#### Message Types

```c
// Message header for all IPC
typedef struct {
    uint16_t type;
    uint16_t subtype;
    uint32_t source_id;
    uint32_t dest_id;
    uint32_t sequence;
    uint64_t timestamp;
    uint32_t data_size;
    uint32_t checksum;
} message_header_t;

// Message type definitions
enum message_types {
    MSG_COMMAND         = 0x0100,
    MSG_STATUS          = 0x0200,
    MSG_TELEMETRY       = 0x0300,
    MSG_SENSOR_DATA     = 0x0400,
    MSG_HEALTH          = 0x0500,
    MSG_LOG             = 0x0600
};

// Command message subtypes
enum command_subtypes {
    CMD_ENGINE_START    = 0x0001,
    CMD_ENGINE_STOP     = 0x0002,
    CMD_ENGINE_THROTTLE = 0x0003,
    CMD_ABORT           = 0x0004,
    CMD_MODE_CHANGE     = 0x0005
};
```

#### Shared Memory Architecture

```c
// Shared memory regions for high-frequency data
typedef struct {
    // Vehicle state (updated by flight control)
    volatile vehicle_state_t vehicle_state;
    
    // Sensor data (updated by sensor subsystem)
    volatile sensor_data_t sensor_data;
    
    // Engine parameters (updated by engine control)
    volatile engine_status_t engine_status;
    
    // Telemetry buffer (circular buffer for logging)
    volatile telemetry_ring_buffer_t telemetry_buffer;
    
    // System health status
    volatile system_health_t system_health;
    
    // Synchronization
    pthread_mutex_t state_mutex;
    pthread_cond_t state_updated;
    
} shared_data_t;

// Memory mapping setup
static shared_data_t *g_shared_data = NULL;

int init_shared_memory(void) {
    int shm_fd = shm_open("/sls_shared", O_RDWR | O_CREAT, 0666);
    if (shm_fd == -1) {
        return -1;
    }
    
    ftruncate(shm_fd, sizeof(shared_data_t));
    
    g_shared_data = mmap(NULL, sizeof(shared_data_t),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED, shm_fd, 0);
    
    if (g_shared_data == MAP_FAILED) {
        close(shm_fd);
        return -1;
    }
    
    // Initialize synchronization primitives
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    
    pthread_mutex_init(&g_shared_data->state_mutex, &mutex_attr);
    
    close(shm_fd);
    return 0;
}
```

## Flight Control System

### Control Loop Architecture

```c
// Flight control system main loop
void* flight_control_loop(void *arg) {
    flight_control_state_t fcs_state = {0};
    control_gains_t gains;
    load_control_gains(&gains);
    
    while (g_simulation_running) {
        uint64_t loop_start = get_microseconds();
        
        // 1. Read sensor data from shared memory
        vehicle_state_t current_state;
        read_vehicle_state(&current_state);
        
        // 2. Execute guidance algorithm
        guidance_commands_t guidance;
        compute_guidance(&current_state, &fcs_state.trajectory, &guidance);
        
        // 3. Execute navigation algorithm  
        navigation_state_t nav_state;
        update_navigation(&current_state, &guidance, &nav_state);
        
        // 4. Execute attitude control
        attitude_commands_t attitude_cmd;
        compute_attitude_control(&current_state, &nav_state, &gains, &attitude_cmd);
        
        // 5. Send commands to engine control
        send_engine_commands(&attitude_cmd);
        
        // 6. Update vehicle dynamics
        update_vehicle_dynamics(&current_state, &attitude_cmd, CONTROL_LOOP_DT);
        
        // 7. Write updated state to shared memory
        write_vehicle_state(&current_state);
        
        // 8. Check timing constraints
        uint64_t loop_end = get_microseconds();
        uint64_t execution_time = loop_end - loop_start;
        
        if (execution_time > FLIGHT_CONTROL_DEADLINE) {
            log_timing_violation("Flight Control", execution_time);
        }
        
        // 9. Wait for next control cycle
        wait_for_control_timer();
    }
    
    return NULL;
}
```

### Guidance Algorithm

```c
// Guidance algorithm for launch trajectory
void compute_guidance(const vehicle_state_t *current_state,
                     const trajectory_t *reference_trajectory,
                     guidance_commands_t *commands) {
    
    // Compute trajectory errors
    vector3_t position_error;
    vector3_subtract(&reference_trajectory->position, 
                     &current_state->position, &position_error);
    
    vector3_t velocity_error;
    vector3_subtract(&reference_trajectory->velocity,
                     &current_state->velocity, &velocity_error);
    
    // PID control for position
    static vector3_t integral_error = {0, 0, 0};
    static vector3_t previous_error = {0, 0, 0};
    
    // Integral term
    vector3_t integral_term;
    vector3_scale(&position_error, CONTROL_LOOP_DT, &integral_term);
    vector3_add(&integral_error, &integral_term, &integral_error);
    
    // Derivative term
    vector3_t derivative_error;
    vector3_subtract(&position_error, &previous_error, &derivative_error);
    vector3_scale(&derivative_error, 1.0/CONTROL_LOOP_DT, &derivative_error);
    
    // Compute commanded acceleration
    vector3_t proportional_term, derivative_term, integral_scaled;
    vector3_scale(&position_error, KP_GUIDANCE, &proportional_term);
    vector3_scale(&derivative_error, KD_GUIDANCE, &derivative_term);
    vector3_scale(&integral_error, KI_GUIDANCE, &integral_scaled);
    
    vector3_add(&proportional_term, &derivative_term, &commands->acceleration);
    vector3_add(&commands->acceleration, &integral_scaled, &commands->acceleration);
    
    // Add gravity compensation
    commands->acceleration.z += 9.81;
    
    // Limit acceleration commands
    double accel_magnitude = vector3_magnitude(&commands->acceleration);
    if (accel_magnitude > MAX_ACCELERATION) {
        vector3_scale(&commands->acceleration, 
                      MAX_ACCELERATION / accel_magnitude, 
                      &commands->acceleration);
    }
    
    previous_error = position_error;
}
```

## Engine Control System

### Engine State Machine

```c
// Engine control state machine
typedef enum {
    ENGINE_STATE_SHUTDOWN,
    ENGINE_STATE_STARTUP,
    ENGINE_STATE_IGNITION,
    ENGINE_STATE_RUNNING,
    ENGINE_STATE_THROTTLING,
    ENGINE_STATE_CUTOFF,
    ENGINE_STATE_FAULT
} engine_state_t;

// State transition function
engine_state_t process_engine_state_machine(engine_state_t current_state,
                                           engine_command_t command,
                                           engine_telemetry_t *telemetry) {
    engine_state_t next_state = current_state;
    
    switch (current_state) {
        case ENGINE_STATE_SHUTDOWN:
            if (command.type == CMD_ENGINE_START) {
                next_state = ENGINE_STATE_STARTUP;
                log_event(LOG_INFO, "Engine startup initiated");
            }
            break;
            
        case ENGINE_STATE_STARTUP:
            // Check pre-start conditions
            if (check_engine_startup_conditions(telemetry)) {
                next_state = ENGINE_STATE_IGNITION;
                initiate_ignition_sequence();
            } else if (get_elapsed_time() > STARTUP_TIMEOUT) {
                next_state = ENGINE_STATE_FAULT;
                log_event(LOG_ERROR, "Engine startup timeout");
            }
            break;
            
        case ENGINE_STATE_IGNITION:
            if (telemetry->chamber_pressure > IGNITION_PRESSURE_THRESHOLD) {
                next_state = ENGINE_STATE_RUNNING;
                log_event(LOG_INFO, "Engine ignition successful");
            } else if (get_elapsed_time() > IGNITION_TIMEOUT) {
                next_state = ENGINE_STATE_FAULT;
                log_event(LOG_ERROR, "Engine ignition failed");
            }
            break;
            
        case ENGINE_STATE_RUNNING:
            if (command.type == CMD_ENGINE_THROTTLE) {
                next_state = ENGINE_STATE_THROTTLING;
            } else if (command.type == CMD_ENGINE_STOP || 
                      command.type == CMD_ABORT) {
                next_state = ENGINE_STATE_CUTOFF;
            } else if (check_engine_fault_conditions(telemetry)) {
                next_state = ENGINE_STATE_FAULT;
            }
            break;
            
        case ENGINE_STATE_THROTTLING:
            if (abs(telemetry->throttle_position - command.throttle_target) < 
                THROTTLE_TOLERANCE) {
                next_state = ENGINE_STATE_RUNNING;
            }
            break;
            
        case ENGINE_STATE_CUTOFF:
            if (telemetry->chamber_pressure < CUTOFF_PRESSURE_THRESHOLD) {
                next_state = ENGINE_STATE_SHUTDOWN;
                log_event(LOG_INFO, "Engine shutdown complete");
            }
            break;
            
        case ENGINE_STATE_FAULT:
            // Stay in fault state until manual reset
            if (command.type == CMD_RESET) {
                next_state = ENGINE_STATE_SHUTDOWN;
                log_event(LOG_INFO, "Engine fault reset");
            }
            break;
    }
    
    return next_state;
}
```

## Sensor System

### Sensor Data Processing

```c
// Sensor fusion for navigation
typedef struct {
    // Inertial Measurement Unit
    vector3_t accelerometer;     // m/s²
    vector3_t gyroscope;         // rad/s
    vector3_t magnetometer;      // μT
    
    // Navigation sensors
    gps_data_t gps;              // Position, velocity
    barometer_data_t barometer;  // Altitude, pressure
    
    // Engine sensors
    double chamber_pressure[MAX_ENGINES];  // Pa
    double nozzle_temperature[MAX_ENGINES]; // K
    double fuel_flow_rate[MAX_ENGINES];     // kg/s
    
    // Environmental sensors
    double ambient_pressure;     // Pa
    double ambient_temperature;  // K
    double wind_speed;          // m/s
    double wind_direction;      // rad
    
    // Health monitoring
    sensor_health_t health[MAX_SENSORS];
    uint64_t timestamp;
} sensor_data_t;

// Kalman filter for sensor fusion
void update_kalman_filter(kalman_state_t *state,
                         const sensor_data_t *sensors) {
    // Prediction step
    matrix_t F = create_state_transition_matrix(CONTROL_LOOP_DT);
    matrix_t Q = create_process_noise_matrix();
    
    // Predict state
    matrix_multiply(&F, &state->x, &state->x_pred);
    
    // Predict covariance
    matrix_t Ft = matrix_transpose(&F);
    matrix_t temp;
    matrix_multiply(&F, &state->P, &temp);
    matrix_multiply(&temp, &Ft, &state->P_pred);
    matrix_add(&state->P_pred, &Q, &state->P_pred);
    
    // Update step
    vector_t z = create_measurement_vector(sensors);
    matrix_t H = create_measurement_matrix();
    matrix_t R = create_measurement_noise_matrix();
    
    // Innovation
    vector_t y;
    matrix_multiply(&H, &state->x_pred, &temp_vec);
    vector_subtract(&z, &temp_vec, &y);
    
    // Innovation covariance
    matrix_t Ht = matrix_transpose(&H);
    matrix_multiply(&H, &state->P_pred, &temp);
    matrix_multiply(&temp, &Ht, &state->S);
    matrix_add(&state->S, &R, &state->S);
    
    // Kalman gain
    matrix_t S_inv = matrix_inverse(&state->S);
    matrix_multiply(&state->P_pred, &Ht, &temp);
    matrix_multiply(&temp, &S_inv, &state->K);
    
    // Update state estimate
    matrix_multiply(&state->K, &y, &temp_vec);
    vector_add(&state->x_pred, &temp_vec, &state->x);
    
    // Update covariance
    matrix_t I = identity_matrix(STATE_DIM);
    matrix_t KH;
    matrix_multiply(&state->K, &H, &KH);
    matrix_subtract(&I, &KH, &temp);
    matrix_multiply(&temp, &state->P_pred, &state->P);
}
```

## Mission Sequencing

### Launch Sequence State Machine

```c
// Mission phase definitions
typedef enum {
    MISSION_PHASE_PRELAUNCH     = 0,
    MISSION_PHASE_COUNTDOWN     = 1,
    MISSION_PHASE_IGNITION      = 2,
    MISSION_PHASE_LIFTOFF       = 3,
    MISSION_PHASE_ASCENT        = 4,
    MISSION_PHASE_BOOSTER_SEP   = 5,
    MISSION_PHASE_SECOND_STAGE  = 6,
    MISSION_PHASE_COAST         = 7,
    MISSION_PHASE_PAYLOAD_SEP   = 8,
    MISSION_PHASE_COMPLETE      = 9,
    MISSION_PHASE_ABORT         = 10
} mission_phase_t;

// Mission timeline
typedef struct {
    mission_phase_t phase;
    double time_offset;         // Seconds from T-0
    const char *description;
    void (*action_func)(void);
} mission_event_t;

static const mission_event_t mission_timeline[] = {
    {MISSION_PHASE_PRELAUNCH,   -7200, "Pre-launch systems check", prelaunch_check},
    {MISSION_PHASE_COUNTDOWN,    -600, "Final countdown sequence", start_countdown},
    {MISSION_PHASE_COUNTDOWN,     -10, "Engine start sequence", engine_start_sequence},
    {MISSION_PHASE_IGNITION,       -3, "Engine ignition", engine_ignition},
    {MISSION_PHASE_LIFTOFF,         0, "Liftoff", liftoff_sequence},
    {MISSION_PHASE_ASCENT,         10, "Clear tower", tower_clear},
    {MISSION_PHASE_ASCENT,         60, "Max Q", max_q_throttle},
    {MISSION_PHASE_BOOSTER_SEP,   150, "Booster separation", booster_separation},
    {MISSION_PHASE_SECOND_STAGE,  155, "Second stage ignition", second_stage_ignition},
    {MISSION_PHASE_COAST,         600, "Second stage cutoff", second_stage_cutoff},
    {MISSION_PHASE_PAYLOAD_SEP,   900, "Payload separation", payload_separation},
    {MISSION_PHASE_COMPLETE,      920, "Mission complete", mission_complete},
    {0, 0, NULL, NULL}  // Terminator
};

// Execute mission sequence
void execute_mission_sequence(double mission_time) {
    static int current_event = 0;
    
    // Check if it's time for the next event
    if (mission_timeline[current_event].action_func != NULL &&
        mission_time >= mission_timeline[current_event].time_offset) {
        
        log_event(LOG_INFO, "T%+.1f: %s", 
                  mission_timeline[current_event].time_offset,
                  mission_timeline[current_event].description);
        
        // Execute the event action
        mission_timeline[current_event].action_func();
        
        // Update mission phase
        g_current_mission_phase = mission_timeline[current_event].phase;
        
        // Move to next event
        current_event++;
    }
}
```

## Performance and Monitoring

### Real-Time Performance Metrics

```c
// Performance monitoring for subsystems
typedef struct {
    uint64_t execution_time_min;
    uint64_t execution_time_max;
    uint64_t execution_time_avg;
    uint32_t loop_count;
    uint32_t overruns;
    double cpu_utilization;
} performance_metrics_t;

void update_performance_metrics(performance_metrics_t *metrics,
                               uint64_t execution_time) {
    metrics->loop_count++;
    
    if (execution_time < metrics->execution_time_min || 
        metrics->execution_time_min == 0) {
        metrics->execution_time_min = execution_time;
    }
    
    if (execution_time > metrics->execution_time_max) {
        metrics->execution_time_max = execution_time;
    }
    
    // Update running average
    metrics->execution_time_avg = 
        (metrics->execution_time_avg * (metrics->loop_count - 1) + execution_time) / 
        metrics->loop_count;
}
```

### Telemetry Data Management

```c
// Telemetry data structure
typedef struct {
    uint32_t packet_id;
    uint64_t timestamp;
    telemetry_type_t type;
    union {
        vehicle_state_t vehicle_data;
        engine_telemetry_t engine_data;
        sensor_data_t sensor_data;
        system_health_t health_data;
    } data;
    uint32_t checksum;
} telemetry_packet_t;

// Circular buffer for telemetry storage
typedef struct {
    telemetry_packet_t *buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t count;
    pthread_mutex_t mutex;
} telemetry_buffer_t;

int store_telemetry_packet(telemetry_buffer_t *buffer,
                          const telemetry_packet_t *packet) {
    pthread_mutex_lock(&buffer->mutex);
    
    if (buffer->count >= buffer->size) {
        // Buffer full, overwrite oldest
        buffer->tail = (buffer->tail + 1) % buffer->size;
    } else {
        buffer->count++;
    }
    
    memcpy(&buffer->buffer[buffer->head], packet, sizeof(*packet));
    buffer->head = (buffer->head + 1) % buffer->size;
    
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}
```

This architecture provides a realistic simulation of space launch systems using QNX's real-time capabilities, demonstrating the same patterns and practices used in actual aerospace applications.
