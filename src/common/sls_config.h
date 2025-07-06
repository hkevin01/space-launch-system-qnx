#ifndef SLS_CONFIG_H
#define SLS_CONFIG_H

#include "sls_types.h"

/**
 * @file sls_config.h
 * @brief Configuration constants and parameters for the Space Launch System
 */

// Timing constants (in milliseconds)
#define MAIN_LOOP_PERIOD_MS 10       // 100 Hz main loop
#define TELEMETRY_PERIOD_MS 100      // 10 Hz telemetry
#define STATUS_REPORT_PERIOD_MS 1000 // 1 Hz status reports
#define HEARTBEAT_PERIOD_MS 5000     // 0.2 Hz heartbeat
#define SENSOR_SAMPLE_PERIOD_MS 50   // 20 Hz sensor sampling

// QNX specific configuration
#define QNX_CHANNEL_NAME_PREFIX "/tmp/sls_"
#define QNX_MAX_CLIENTS 16
#define QNX_MESSAGE_QUEUE_SIZE 256
#define QNX_THREAD_STACK_SIZE (64 * 1024)

// Vehicle parameters
#define VEHICLE_DRY_MASS_KG 500000.0   // 500 tons
#define VEHICLE_FUEL_MASS_KG 1500000.0 // 1500 tons
#define VEHICLE_MAX_THRUST_N 7500000.0 // 7.5 MN
#define VEHICLE_MAX_THROTTLE 100.0     // 100%
#define VEHICLE_MIN_THROTTLE 60.0      // 60%

// Engine parameters
#define NUM_ENGINES 4
#define ENGINE_STARTUP_TIME_S 3.0
#define ENGINE_SHUTDOWN_TIME_S 2.0
#define ENGINE_MAX_CHAMBER_PRESSURE 20000000.0 // 20 MPa
#define ENGINE_NOMINAL_ISP 450.0               // seconds

// Launch sequence timing
#define T_MINUS_HOLD_POINTS {-3600, -1800, -600, -60, -10}
#define T_MINUS_ENGINE_START -6.0 // T-6 seconds
#define T_ZERO_LIFTOFF 0.0
#define T_PLUS_STAGE_SEP 120.0    // T+2 minutes
#define T_PLUS_ORBIT_INSERT 480.0 // T+8 minutes

// Sensor limits and ranges
#define TEMP_MIN_K 200.0           // Minimum temperature
#define TEMP_MAX_K 2000.0          // Maximum temperature
#define PRESSURE_MIN_PA 0.0        // Minimum pressure
#define PRESSURE_MAX_PA 50000000.0 // 50 MPa max pressure
#define ACCEL_MAX_G 10.0           // Maximum acceleration
#define VIBRATION_MAX_G 5.0        // Maximum vibration

// Communication parameters
#define TELEMETRY_BUFFER_SIZE 4096
#define COMMAND_BUFFER_SIZE 1024
#define LOG_BUFFER_SIZE 8192
#define TELEMETRY_RATE_HZ 10
#define COMMAND_TIMEOUT_MS 5000

// Safety limits
#define MAX_ABORT_TIME_S 300.0        // 5 minutes max abort time
#define FUEL_RESERVE_PERCENTAGE 5.0   // 5% fuel reserve
#define PRESSURE_SAFETY_MARGIN 1.2    // 20% safety margin
#define TEMPERATURE_SAFETY_MARGIN 1.1 // 10% safety margin

// Environmental limits for launch
#define MAX_WIND_SPEED_MS 15.0     // 15 m/s max wind
#define MAX_PRECIPITATION_MMHR 1.0 // 1 mm/hr max rain
#define MIN_VISIBILITY_M 5000.0    // 5 km minimum visibility
#define LIGHTNING_STANDOFF_KM 16.0 // 16 km lightning standoff

// Fault detection parameters
#define SENSOR_FAULT_THRESHOLD 3      // 3 consecutive bad readings
#define COMMUNICATION_TIMEOUT_MS 2000 // 2 second comm timeout
#define WATCHDOG_TIMEOUT_MS 1000      // 1 second watchdog
#define MAX_FAULT_RECOVERY_ATTEMPTS 3

// File paths
#define CONFIG_FILE_PATH "config/system.conf"
#define LOG_FILE_PATH "logs/sls_simulation.log"
#define TELEMETRY_FILE_PATH "logs/telemetry.csv"
#define MISSION_PROFILE_PATH "config/mission_profile.json"

// Network configuration
#define TELEMETRY_PORT 8080
#define COMMAND_PORT 8081
#define STATUS_PORT 8082
#define MULTICAST_GROUP "239.1.1.1"

// UI Configuration
#define UI_UPDATE_RATE_HZ 30      // 30 FPS UI updates
#define PLOT_HISTORY_SECONDS 300  // 5 minutes of plot history
#define ALARM_FLASH_PERIOD_MS 500 // 500ms alarm flash period

// Mission phases configuration
typedef struct
{
    mission_phase_t phase;
    double start_time; // Mission time in seconds
    double duration;   // Phase duration in seconds
    char description[MAX_NAME_LENGTH];
    priority_level_t criticality;
} phase_config_t;

// Default mission phases
#define DEFAULT_MISSION_PHASES                                                            \
    {                                                                                     \
        {PHASE_PRELAUNCH, -7200.0, 7200.0, "Pre-launch preparations", PRIORITY_NORMAL},   \
            {PHASE_IGNITION, -6.0, 6.0, "Engine ignition sequence", PRIORITY_CRITICAL},   \
            {PHASE_LIFTOFF, 0.0, 10.0, "Liftoff and initial ascent", PRIORITY_CRITICAL},  \
            {PHASE_ASCENT, 10.0, 110.0, "Atmospheric ascent", PRIORITY_HIGH},             \
            {PHASE_STAGE_SEPARATION, 120.0, 5.0, "Stage separation", PRIORITY_HIGH},      \
            {PHASE_ORBIT_INSERTION, 125.0, 355.0, "Orbit insertion burn", PRIORITY_HIGH}, \
        {                                                                                 \
            PHASE_MISSION_COMPLETE, 480.0, 0.0, "Mission complete", PRIORITY_NORMAL       \
        }                                                                                 \
    }

// Subsystem configuration
typedef struct
{
    subsystem_type_t type;
    char name[MAX_NAME_LENGTH];
    priority_level_t priority;
    uint32_t update_rate_hz;
    bool fault_tolerant;
    bool has_redundancy;
    uint32_t num_sensors;
} subsystem_config_t;

// Default subsystem configurations
#define DEFAULT_SUBSYSTEM_CONFIGS {                                                             \
    {SUBSYS_FLIGHT_CONTROL, "Flight Control Computer", PRIORITY_CRITICAL, 100, true, true, 12}, \
    {SUBSYS_ENGINE_CONTROL, "Engine Control System", PRIORITY_CRITICAL, 50, true, true, 16},    \
    {SUBSYS_TELEMETRY, "Telemetry & Communications", PRIORITY_HIGH, 10, true, false, 8},        \
    {SUBSYS_ENVIRONMENTAL, "Environmental Monitoring", PRIORITY_NORMAL, 5, false, false, 20},   \
    {SUBSYS_GROUND_SUPPORT, "Ground Support Interface", PRIORITY_NORMAL, 1, false, false, 4},   \
    {SUBSYS_NAVIGATION, "Navigation System", PRIORITY_HIGH, 20, true, true, 6},                 \
    {SUBSYS_POWER, "Power Management", PRIORITY_HIGH, 10, true, true, 10},                      \
    {SUBSYS_THERMAL, "Thermal Control", PRIORITY_NORMAL, 2, false, false, 15}}

#endif // SLS_CONFIG_H
