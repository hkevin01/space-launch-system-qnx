#ifndef SLS_TYPES_H
#define SLS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @file sls_types.h
 * @brief Common data types and structures for Space Launch System simulation
 */

// System-wide constants
#define MAX_SUBSYSTEMS 16
#define MAX_SENSORS 64
#define MAX_TELEMETRY_POINTS 256
#define MAX_NAME_LENGTH 64
#define MAX_MESSAGE_LENGTH 512

// Mission phases
typedef enum
{
    PHASE_PRELAUNCH = 0,
    PHASE_IGNITION,
    PHASE_LIFTOFF,
    PHASE_ASCENT,
    PHASE_STAGE_SEPARATION,
    PHASE_ORBIT_INSERTION,
    PHASE_MISSION_COMPLETE,
    PHASE_ABORT,
    PHASE_UNKNOWN
} mission_phase_t;

// System states
typedef enum
{
    STATE_OFFLINE = 0,
    STATE_INITIALIZING,
    STATE_STANDBY,
    STATE_ACTIVE,
    STATE_FAULT,
    STATE_EMERGENCY,
    STATE_SHUTDOWN
} system_state_t;

// Subsystem types
typedef enum
{
    SUBSYS_FLIGHT_CONTROL = 0,
    SUBSYS_ENGINE_CONTROL,
    SUBSYS_TELEMETRY,
    SUBSYS_ENVIRONMENTAL,
    SUBSYS_GROUND_SUPPORT,
    SUBSYS_NAVIGATION,
    SUBSYS_POWER,
    SUBSYS_THERMAL
} subsystem_type_t;

// Priority levels for QNX scheduling
typedef enum
{
    PRIORITY_LOW = 10,
    PRIORITY_NORMAL = 20,
    PRIORITY_HIGH = 30,
    PRIORITY_CRITICAL = 40,
    PRIORITY_EMERGENCY = 50
} priority_level_t;

// Sensor data types
typedef enum
{
    SENSOR_TEMPERATURE = 0,
    SENSOR_PRESSURE,
    SENSOR_ACCELERATION,
    SENSOR_VIBRATION,
    SENSOR_FLOW_RATE,
    SENSOR_VOLTAGE,
    SENSOR_CURRENT,
    SENSOR_POSITION,
    SENSOR_VELOCITY,
    SENSOR_ANGULAR_RATE,
    SENSOR_ALTITUDE
} sensor_type_t;

// Telemetry data point
typedef struct
{
    uint32_t id;
    char name[MAX_NAME_LENGTH];
    sensor_type_t type;
    double value;
    double min_value;
    double max_value;
    char units[16];
    struct timespec timestamp;
    bool valid;
    uint32_t quality;
} telemetry_point_t;

// Sensor data structure
typedef struct
{
    uint32_t sensor_id;
    subsystem_type_t subsystem;
    sensor_type_t type;
    char name[MAX_NAME_LENGTH];
    double value;
    double calibration_offset;
    double calibration_scale;
    bool fault_detected;
    struct timespec last_update;
} sensor_data_t;

// Command structure
typedef struct
{
    uint32_t command_id;
    subsystem_type_t target_subsystem;
    char command[MAX_NAME_LENGTH];
    void *parameters;
    size_t param_size;
    priority_level_t priority;
    struct timespec timestamp;
    bool urgent;
} command_t;

// Status message
typedef struct
{
    subsystem_type_t source;
    system_state_t state;
    mission_phase_t phase;
    char message[MAX_MESSAGE_LENGTH];
    priority_level_t priority;
    struct timespec timestamp;
    uint32_t error_code;
} status_message_t;

// Vehicle state
typedef struct
{
    // Position and orientation
    double position[3];         // X, Y, Z in meters
    double velocity[3];         // Vx, Vy, Vz in m/s
    double acceleration[3];     // Ax, Ay, Az in m/s²
    double quaternion[4];       // Orientation quaternion
    double angular_velocity[3]; // Roll, pitch, yaw rates in rad/s

    // Mission parameters
    double mission_time;   // Seconds since T-0
    double fuel_remaining; // Percentage
    double thrust;         // Newtons
    double mass;           // Kilograms

    // Environmental
    double altitude;         // Meters above sea level
    double dynamic_pressure; // Pascal
    double mach_number;      // Mach

    struct timespec timestamp;
} vehicle_state_t;

// Engine parameters
typedef struct
{
    double thrust_percentage;  // 0-100%
    double chamber_pressure;   // Pascal
    double fuel_flow_rate;     // kg/s
    double oxidizer_flow_rate; // kg/s
    double nozzle_temperature; // Kelvin
    bool ignition_enabled;
    bool throttle_enabled;
    struct timespec timestamp;
} engine_state_t;

// Environmental conditions
typedef struct
{
    double temperature;    // Kelvin
    double pressure;       // Pascal
    double humidity;       // Percentage
    double wind_speed;     // m/s
    double wind_direction; // Degrees
    double precipitation;  // mm/hr
    struct timespec timestamp;
} environmental_data_t;

// Communication message types
typedef enum
{
    MSG_TELEMETRY = 0,
    MSG_COMMAND,
    MSG_STATUS,
    MSG_ALARM,
    MSG_HEARTBEAT,
    MSG_LOG
} message_type_t;

// IPC message structure
typedef struct
{
    message_type_t type;
    subsystem_type_t source;
    subsystem_type_t destination;
    uint32_t sequence_number;
    size_t data_length;
    struct timespec timestamp;
    uint8_t data[]; // Variable length data
} ipc_message_t;

// Fault information
typedef struct
{
    uint32_t fault_id;
    subsystem_type_t subsystem;
    char description[MAX_MESSAGE_LENGTH];
    priority_level_t severity;
    bool recoverable;
    bool operator_action_required;
    struct timespec detected_time;
    struct timespec resolved_time;
} fault_info_t;

// Go/No-Go status
typedef struct
{
    subsystem_type_t subsystem;
    bool go_status;
    char reason[MAX_MESSAGE_LENGTH];
    struct timespec timestamp;
} go_nogo_status_t;

#endif // SLS_TYPES_H
