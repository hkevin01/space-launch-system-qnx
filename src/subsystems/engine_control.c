/**
 * @file engine_control.c
 * @brief Engine Control System subsystem for Space Launch System
 *
 * This subsystem manages rocket engine operation including ignition sequence,
 * throttle control, fuel flow management, and engine health monitoring.
 */

#include "../common/sls_types.h"
#include "../common/sls_config.h"
#include "../common/sls_utils.h"
#include "../common/sls_ipc.h"
#include "../common/sls_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Engine states
typedef enum
{
    ENGINE_STATE_OFFLINE = 0,
    ENGINE_STATE_PRESTART,
    ENGINE_STATE_IGNITION,
    ENGINE_STATE_RUNNING,
    ENGINE_STATE_SHUTDOWN,
    ENGINE_STATE_FAULT
} engine_state_t;

// Individual engine data
typedef struct
{
    int engine_id;
    engine_state_t state;
    engine_state_t engine_params;
    double ignition_time;
    double shutdown_time;
    bool fault_detected;
    char fault_message[MAX_MESSAGE_LENGTH];
    sensor_data_t sensors[16]; // Engine sensors
    int num_sensors;
} engine_data_t;

// Engine control system state
typedef struct
{
    engine_data_t engines[NUM_ENGINES];
    mission_phase_t current_phase;
    bool ignition_sequence_active;
    bool shutdown_sequence_active;
    double total_thrust_commanded;
    double total_thrust_actual;
    double fuel_manifold_pressure;
    double oxidizer_manifold_pressure;
    double turbopump_speed[NUM_ENGINES];
    struct timespec last_update;
} engine_control_state_t;

// Global engine control state
static engine_control_state_t g_ecs_state;
static volatile bool g_ecs_shutdown = false;

// Internal function declarations
static void initialize_engine_control(void);
static void initialize_engine(int engine_id);
static void update_engine_sensors(int engine_id, double dt);
static void process_ignition_sequence(double dt);
static void process_shutdown_sequence(double dt);
static void update_engine_state(int engine_id, double dt);
static void calculate_fuel_flow(int engine_id);
static void monitor_engine_health(int engine_id);
static void handle_engine_fault(int engine_id, const char *fault_msg);
static double simulate_chamber_pressure(int engine_id);
static double simulate_turbopump_speed(int engine_id);

/**
 * @brief Engine Control System thread main function
 */
void *engine_control_thread(void *arg)
{
    subsystem_config_t *config = (subsystem_config_t *)arg;
    sls_set_thread_name("EngineControl");

    sls_log(LOG_LEVEL_INFO, "ECS", "Engine Control System started (priority %d)",
            config->priority);

    initialize_engine_control();

    struct timespec loop_start, loop_end, sleep_time;
    const long loop_period_ns = (1000000000L / config->update_rate_hz);

    while (!g_ecs_shutdown)
    {
        clock_gettime(CLOCK_MONOTONIC, &loop_start);

        // Calculate time delta
        double dt = sls_time_diff(&g_ecs_state.last_update, &loop_start);
        g_ecs_state.last_update = loop_start;

        // Process ignition sequence if active
        if (g_ecs_state.ignition_sequence_active)
        {
            process_ignition_sequence(dt);
        }

        // Process shutdown sequence if active
        if (g_ecs_state.shutdown_sequence_active)
        {
            process_shutdown_sequence(dt);
        }

        // Update each engine
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            update_engine_state(i, dt);
            update_engine_sensors(i, dt);
            monitor_engine_health(i);
        }

        // Send telemetry for engines
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            engine_data_t *engine = &g_ecs_state.engines[i];

            // Chamber pressure telemetry
            telemetry_point_t chamber_pressure_telem = {
                .id = 2000 + i * 10,
                .type = SENSOR_PRESSURE,
                .value = engine->engine_params.chamber_pressure,
                .min_value = 0.0,
                .max_value = ENGINE_MAX_CHAMBER_PRESSURE,
                .timestamp = loop_start,
                .valid = !engine->fault_detected,
                .quality = engine->fault_detected ? 50 : 100};
            snprintf(chamber_pressure_telem.name, sizeof(chamber_pressure_telem.name),
                     "Engine%d_ChamberPressure", i + 1);
            strcpy(chamber_pressure_telem.units, "Pa");
            sls_ipc_broadcast_telemetry(&chamber_pressure_telem);

            // Thrust percentage telemetry
            telemetry_point_t thrust_telem = {
                .id = 2001 + i * 10,
                .type = SENSOR_FLOW_RATE,
                .value = engine->engine_params.thrust_percentage,
                .min_value = 0.0,
                .max_value = 100.0,
                .timestamp = loop_start,
                .valid = !engine->fault_detected,
                .quality = engine->fault_detected ? 50 : 100};
            snprintf(thrust_telem.name, sizeof(thrust_telem.name),
                     "Engine%d_ThrustPct", i + 1);
            strcpy(thrust_telem.units, "%");
            sls_ipc_broadcast_telemetry(&thrust_telem);
        }

        // Calculate sleep time
        clock_gettime(CLOCK_MONOTONIC, &loop_end);
        long elapsed_ns = (loop_end.tv_sec - loop_start.tv_sec) * 1000000000L +
                          (loop_end.tv_nsec - loop_start.tv_nsec);

        if (elapsed_ns < loop_period_ns)
        {
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = loop_period_ns - elapsed_ns;
            nanosleep(&sleep_time, NULL);
        }
    }

    sls_log(LOG_LEVEL_INFO, "ECS", "Engine Control System thread terminated");
    return NULL;
}

/**
 * @brief Initialize engine control system
 */
static void initialize_engine_control(void)
{
    memset(&g_ecs_state, 0, sizeof(g_ecs_state));

    // Initialize all engines
    for (int i = 0; i < NUM_ENGINES; i++)
    {
        initialize_engine(i);
    }

    g_ecs_state.current_phase = PHASE_PRELAUNCH;
    g_ecs_state.fuel_manifold_pressure = 1000000.0;     // 1 MPa
    g_ecs_state.oxidizer_manifold_pressure = 1200000.0; // 1.2 MPa

    clock_gettime(CLOCK_MONOTONIC, &g_ecs_state.last_update);

    sls_log(LOG_LEVEL_INFO, "ECS", "Engine control system initialized - %d engines", NUM_ENGINES);
}

/**
 * @brief Initialize individual engine
 */
static void initialize_engine(int engine_id)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    engine->engine_id = engine_id + 1;
    engine->state = ENGINE_STATE_OFFLINE;
    engine->fault_detected = false;
    engine->ignition_time = 0.0;
    engine->shutdown_time = 0.0;

    // Initialize engine parameters
    engine->engine_params.thrust_percentage = 0.0;
    engine->engine_params.chamber_pressure = 101325.0; // Atmospheric pressure
    engine->engine_params.fuel_flow_rate = 0.0;
    engine->engine_params.oxidizer_flow_rate = 0.0;
    engine->engine_params.nozzle_temperature = 300.0; // Room temperature
    engine->engine_params.ignition_enabled = false;
    engine->engine_params.throttle_enabled = true;

    // Initialize turbopump speed
    g_ecs_state.turbopump_speed[engine_id] = 0.0;

    sls_log(LOG_LEVEL_DEBUG, "ECS", "Engine %d initialized", engine_id + 1);
}

/**
 * @brief Update engine sensors
 */
static void update_engine_sensors(int engine_id, double dt)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    // Simulate chamber pressure sensor
    engine->engine_params.chamber_pressure = simulate_chamber_pressure(engine_id);

    // Simulate turbopump speed
    g_ecs_state.turbopump_speed[engine_id] = simulate_turbopump_speed(engine_id);

    // Simulate nozzle temperature
    if (engine->state == ENGINE_STATE_RUNNING)
    {
        engine->engine_params.nozzle_temperature = 2500.0 +
                                                   sls_simulate_sensor_noise(0.0, 50.0);
    }
    else
    {
        engine->engine_params.nozzle_temperature = 300.0 +
                                                   sls_simulate_sensor_noise(0.0, 5.0);
    }

    // Update fuel and oxidizer flow rates
    calculate_fuel_flow(engine_id);

    clock_gettime(CLOCK_REALTIME, &engine->engine_params.timestamp);
}

/**
 * @brief Process ignition sequence
 */
static void process_ignition_sequence(double dt)
{
    static double sequence_timer = 0.0;
    sequence_timer += dt;

    // Ignition sequence stages
    if (sequence_timer < 1.0)
    {
        // Stage 1: Purge and pressurize
        sls_log(LOG_LEVEL_INFO, "ECS", "Ignition sequence: Purging and pressurizing");
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            g_ecs_state.engines[i].state = ENGINE_STATE_PRESTART;
        }
    }
    else if (sequence_timer < 3.0)
    {
        // Stage 2: Spin up turbopumps
        sls_log(LOG_LEVEL_INFO, "ECS", "Ignition sequence: Turbopump startup");
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            g_ecs_state.turbopump_speed[i] = (sequence_timer - 1.0) / 2.0 * 12000.0; // RPM
        }
    }
    else if (sequence_timer < 4.0)
    {
        // Stage 3: Ignition
        sls_log(LOG_LEVEL_INFO, "ECS", "Ignition sequence: Engine ignition");
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            g_ecs_state.engines[i].state = ENGINE_STATE_IGNITION;
            g_ecs_state.engines[i].engine_params.ignition_enabled = true;
        }
    }
    else
    {
        // Stage 4: Ramp to full thrust
        sls_log(LOG_LEVEL_INFO, "ECS", "Ignition sequence: Thrust ramp-up");
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            if (g_ecs_state.engines[i].state == ENGINE_STATE_IGNITION)
            {
                g_ecs_state.engines[i].state = ENGINE_STATE_RUNNING;
            }
        }
        g_ecs_state.ignition_sequence_active = false;
        sequence_timer = 0.0;
        sls_log(LOG_LEVEL_INFO, "ECS", "Ignition sequence complete - all engines running");
    }
}

/**
 * @brief Process shutdown sequence
 */
static void process_shutdown_sequence(double dt)
{
    static double sequence_timer = 0.0;
    sequence_timer += dt;

    if (sequence_timer < ENGINE_SHUTDOWN_TIME_S)
    {
        // Gradual thrust reduction
        double thrust_factor = 1.0 - (sequence_timer / ENGINE_SHUTDOWN_TIME_S);
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            if (g_ecs_state.engines[i].state == ENGINE_STATE_RUNNING)
            {
                g_ecs_state.engines[i].engine_params.thrust_percentage =
                    VEHICLE_MIN_THROTTLE * thrust_factor;
            }
        }
    }
    else
    {
        // Complete shutdown
        for (int i = 0; i < NUM_ENGINES; i++)
        {
            g_ecs_state.engines[i].state = ENGINE_STATE_OFFLINE;
            g_ecs_state.engines[i].engine_params.thrust_percentage = 0.0;
            g_ecs_state.engines[i].engine_params.ignition_enabled = false;
        }
        g_ecs_state.shutdown_sequence_active = false;
        sequence_timer = 0.0;
        sls_log(LOG_LEVEL_INFO, "ECS", "Engine shutdown sequence complete");
    }
}

/**
 * @brief Update individual engine state
 */
static void update_engine_state(int engine_id, double dt)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    switch (engine->state)
    {
    case ENGINE_STATE_OFFLINE:
        engine->engine_params.thrust_percentage = 0.0;
        engine->engine_params.ignition_enabled = false;
        break;

    case ENGINE_STATE_PRESTART:
        // Preparing for ignition
        engine->engine_params.thrust_percentage = 0.0;
        break;

    case ENGINE_STATE_IGNITION:
        // Ignition in progress
        engine->ignition_time += dt;
        if (engine->ignition_time > 1.0)
        { // 1 second ignition delay
            engine->state = ENGINE_STATE_RUNNING;
            engine->engine_params.thrust_percentage = VEHICLE_MIN_THROTTLE;
            sls_log(LOG_LEVEL_INFO, "ECS", "Engine %d ignited successfully",
                    engine->engine_id);
        }
        break;

    case ENGINE_STATE_RUNNING:
        // Normal operation - thrust can be commanded
        if (g_ecs_state.current_phase >= PHASE_LIFTOFF)
        {
            // Ramp up to full thrust
            if (engine->engine_params.thrust_percentage < 100.0)
            {
                engine->engine_params.thrust_percentage += 20.0 * dt; // 20%/sec ramp
                engine->engine_params.thrust_percentage =
                    sls_clamp(engine->engine_params.thrust_percentage, 0.0, 100.0);
            }
        }
        break;

    case ENGINE_STATE_SHUTDOWN:
        // Shutdown in progress
        engine->shutdown_time += dt;
        if (engine->shutdown_time > ENGINE_SHUTDOWN_TIME_S)
        {
            engine->state = ENGINE_STATE_OFFLINE;
            sls_log(LOG_LEVEL_INFO, "ECS", "Engine %d shutdown complete",
                    engine->engine_id);
        }
        break;

    case ENGINE_STATE_FAULT:
        // Engine fault detected
        engine->engine_params.thrust_percentage = 0.0;
        engine->engine_params.ignition_enabled = false;
        break;
    }
}

/**
 * @brief Calculate fuel flow rates
 */
static void calculate_fuel_flow(int engine_id)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    if (engine->state == ENGINE_STATE_RUNNING)
    {
        // Base flow rates at 100% thrust
        double base_fuel_flow = 200.0;     // kg/s per engine
        double base_oxidizer_flow = 400.0; // kg/s per engine (LOX)

        // Scale by thrust percentage
        double thrust_factor = engine->engine_params.thrust_percentage / 100.0;
        engine->engine_params.fuel_flow_rate = base_fuel_flow * thrust_factor;
        engine->engine_params.oxidizer_flow_rate = base_oxidizer_flow * thrust_factor;
    }
    else
    {
        engine->engine_params.fuel_flow_rate = 0.0;
        engine->engine_params.oxidizer_flow_rate = 0.0;
    }
}

/**
 * @brief Monitor engine health and detect faults
 */
static void monitor_engine_health(int engine_id)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    // Check chamber pressure limits
    if (engine->state == ENGINE_STATE_RUNNING)
    {
        if (engine->engine_params.chamber_pressure > ENGINE_MAX_CHAMBER_PRESSURE)
        {
            handle_engine_fault(engine_id, "Chamber pressure exceeded maximum");
            return;
        }

        if (engine->engine_params.chamber_pressure < 1000000.0)
        { // 1 MPa minimum
            handle_engine_fault(engine_id, "Chamber pressure too low");
            return;
        }
    }

    // Check turbopump speed
    if (engine->state == ENGINE_STATE_RUNNING)
    {
        if (g_ecs_state.turbopump_speed[engine_id] < 8000.0)
        { // RPM
            handle_engine_fault(engine_id, "Turbopump underspeed");
            return;
        }
    }

    // Check nozzle temperature
    if (engine->engine_params.nozzle_temperature > 3000.0)
    {
        handle_engine_fault(engine_id, "Nozzle overtemperature");
        return;
    }

    // Random fault injection for testing (very low probability)
    if (sls_simulate_sensor_fault(0.0001))
    { // 0.01% chance per update
        handle_engine_fault(engine_id, "Random fault injection");
    }
}

/**
 * @brief Handle engine fault condition
 */
static void handle_engine_fault(int engine_id, const char *fault_msg)
{
    if (engine_id >= NUM_ENGINES)
    {
        return;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];

    if (!engine->fault_detected)
    {
        engine->fault_detected = true;
        engine->state = ENGINE_STATE_FAULT;
        sls_safe_strncpy(engine->fault_message, fault_msg, sizeof(engine->fault_message));

        sls_log(LOG_LEVEL_ERROR, "ECS", "Engine %d FAULT: %s",
                engine->engine_id, fault_msg);

        // Send fault notification
        status_message_t fault_status = {
            .source = SUBSYS_ENGINE_CONTROL,
            .state = STATE_FAULT,
            .phase = g_ecs_state.current_phase,
            .priority = PRIORITY_CRITICAL,
            .error_code = 3000 + engine_id};
        snprintf(fault_status.message, sizeof(fault_status.message),
                 "Engine %d fault: %s", engine->engine_id, fault_msg);
        clock_gettime(CLOCK_REALTIME, &fault_status.timestamp);

        sls_ipc_broadcast_status(&fault_status);
    }
}

/**
 * @brief Simulate chamber pressure
 */
static double simulate_chamber_pressure(int engine_id)
{
    if (engine_id >= NUM_ENGINES)
    {
        return 101325.0;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];
    double base_pressure = 101325.0; // Atmospheric

    if (engine->state == ENGINE_STATE_RUNNING)
    {
        // Scale pressure with thrust percentage
        double thrust_factor = engine->engine_params.thrust_percentage / 100.0;
        base_pressure = 101325.0 + (ENGINE_MAX_CHAMBER_PRESSURE - 101325.0) * thrust_factor;
    }

    // Add some noise
    return sls_simulate_sensor_noise(base_pressure, base_pressure * 0.02); // 2% noise
}

/**
 * @brief Simulate turbopump speed
 */
static double simulate_turbopump_speed(int engine_id)
{
    if (engine_id >= NUM_ENGINES)
    {
        return 0.0;
    }

    engine_data_t *engine = &g_ecs_state.engines[engine_id];
    double base_speed = 0.0;

    if (engine->state == ENGINE_STATE_RUNNING)
    {
        double thrust_factor = engine->engine_params.thrust_percentage / 100.0;
        base_speed = 8000.0 + 4000.0 * thrust_factor; // 8000-12000 RPM range
    }

    // Add some noise
    return sls_simulate_sensor_noise(base_speed, base_speed * 0.05); // 5% noise
}
