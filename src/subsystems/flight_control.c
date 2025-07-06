/**
 * @file flight_control.c
 * @brief Flight Control Computer subsystem for Space Launch System
 *
 * This subsystem handles primary flight control, navigation guidance,
 * and mission sequencing for the space launch vehicle.
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

// Flight control state
typedef struct
{
    vehicle_state_t vehicle_state;
    mission_phase_t current_phase;
    bool autopilot_enabled;
    bool guidance_active;
    double target_altitude;
    double target_velocity[3];
    double control_gains[3]; // PID gains
    double last_error[3];
    double integral_error[3];
    struct timespec last_update;
} flight_control_state_t;

// Global flight control state
static flight_control_state_t g_fc_state;
static volatile bool g_fc_shutdown = false;

// Internal function declarations
static void initialize_flight_control(void);
static void update_vehicle_dynamics(double dt);
static void calculate_guidance_commands(void);
static void update_autopilot(double dt);
static void handle_mission_phase_change(mission_phase_t new_phase);
static void simulate_atmospheric_effects(void);
static void check_flight_constraints(void);
static void process_status_updates(void); // New function declaration

/**
 * @brief Flight Control Computer thread main function
 */
void *flight_control_thread(void *arg)
{
    subsystem_config_t *config = (subsystem_config_t *)arg;
    sls_set_thread_name("FlightControl");

    sls_log(LOG_LEVEL_INFO, "FCC", "Flight Control Computer started (priority %d)",
            config->priority);

    initialize_flight_control();

    struct timespec loop_start, loop_end, sleep_time;
    const long loop_period_ns = (1000000000L / config->update_rate_hz);

    while (!g_fc_shutdown)
    {
        clock_gettime(CLOCK_MONOTONIC, &loop_start);

        // Calculate time delta
        double dt = sls_time_diff(&g_fc_state.last_update, &loop_start);
        g_fc_state.last_update = loop_start;

        // Process incoming status updates (including phase changes)
        process_status_updates();

        // Update vehicle dynamics simulation
        update_vehicle_dynamics(dt);

        // Calculate guidance commands if in active flight
        if (g_fc_state.current_phase >= PHASE_LIFTOFF &&
            g_fc_state.current_phase <= PHASE_ORBIT_INSERTION)
        {
            calculate_guidance_commands();
        }

        // Run autopilot if enabled
        if (g_fc_state.autopilot_enabled)
        {
            update_autopilot(dt);
        }

        // Apply atmospheric effects
        simulate_atmospheric_effects();

        // Check flight safety constraints
        check_flight_constraints();

        // Send telemetry
        telemetry_point_t telemetry = {
            .id = 1000,
            .type = SENSOR_POSITION,
            .value = g_fc_state.vehicle_state.altitude,
            .min_value = -1000.0,
            .max_value = 1000000.0,
            .timestamp = loop_start,
            .valid = true,
            .quality = 100};
        strcpy(telemetry.name, "Altitude");
        strcpy(telemetry.units, "m");
        sls_ipc_broadcast_telemetry(&telemetry);

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

    sls_log(LOG_LEVEL_INFO, "FCC", "Flight Control Computer thread terminated");
    return NULL;
}

/**
 * @brief Initialize flight control system
 */
static void initialize_flight_control(void)
{
    memset(&g_fc_state, 0, sizeof(g_fc_state));

    // Initialize vehicle state at launch pad
    g_fc_state.vehicle_state.position[0] = 0.0; // X (downrange)
    g_fc_state.vehicle_state.position[1] = 0.0; // Y (crossrange)
    g_fc_state.vehicle_state.position[2] = 0.0; // Z (altitude)
    g_fc_state.vehicle_state.altitude = 0.0;
    g_fc_state.vehicle_state.mass = VEHICLE_DRY_MASS_KG + VEHICLE_FUEL_MASS_KG;
    g_fc_state.vehicle_state.fuel_remaining = 100.0;

    // Initialize orientation (pointing up)
    g_fc_state.vehicle_state.quaternion[0] = 1.0; // w
    g_fc_state.vehicle_state.quaternion[1] = 0.0; // x
    g_fc_state.vehicle_state.quaternion[2] = 0.0; // y
    g_fc_state.vehicle_state.quaternion[3] = 0.0; // z

    // Initialize control parameters
    g_fc_state.autopilot_enabled = true;
    g_fc_state.guidance_active = false;
    g_fc_state.target_altitude = 400000.0; // 400 km target orbit

    // PID gains for altitude control
    g_fc_state.control_gains[0] = 0.1;  // Proportional
    g_fc_state.control_gains[1] = 0.01; // Integral
    g_fc_state.control_gains[2] = 0.05; // Derivative

    clock_gettime(CLOCK_MONOTONIC, &g_fc_state.last_update);

    sls_log(LOG_LEVEL_INFO, "FCC", "Flight control initialized - vehicle mass: %.0f kg",
            g_fc_state.vehicle_state.mass);
}

/**
 * @brief Update vehicle dynamics simulation
 */
static void update_vehicle_dynamics(double dt)
{
    if (dt <= 0.0 || dt > 1.0)
    { // Sanity check
        return;
    }

    vehicle_state_t *vs = &g_fc_state.vehicle_state;

    // Update mission time
    vs->mission_time += dt;

    // Apply physics based on mission phase and ground support
    if (g_fc_state.current_phase >= PHASE_LIFTOFF &&
        g_fc_state.current_phase <= PHASE_ORBIT_INSERTION)
    {
        // Vehicle is in flight - apply thrust and gravity
        
        // Calculate thrust based on mission phase
        double thrust_percentage = 100.0;
        if (g_fc_state.current_phase == PHASE_ASCENT)
        {
            // Throttle down during atmospheric ascent
            thrust_percentage = 75.0;
        }

        vs->thrust = VEHICLE_MAX_THRUST_N * (thrust_percentage / 100.0);

        // Calculate acceleration (F = ma)
        double thrust_accel = vs->thrust / vs->mass;
        vs->acceleration[2] = thrust_accel - 9.81; // Thrust minus gravity

        // Fuel consumption
        double fuel_flow_rate = 1000.0; // kg/s (simplified)
        vs->mass -= fuel_flow_rate * dt;
        vs->fuel_remaining = ((vs->mass - VEHICLE_DRY_MASS_KG) / VEHICLE_FUEL_MASS_KG) * 100.0;
        vs->fuel_remaining = sls_clamp(vs->fuel_remaining, 0.0, 100.0);
    }
    else if (g_fc_state.current_phase == PHASE_IGNITION)
    {
        // Vehicle is igniting engines but still on ground support
        vs->thrust = VEHICLE_MAX_THRUST_N * 0.5; // 50% thrust during ignition
        
        // Ground support counteracts gravity - vehicle stays at ground level
        vs->acceleration[0] = 0.0;
        vs->acceleration[1] = 0.0;
        vs->acceleration[2] = 0.0;
        vs->velocity[0] = 0.0;
        vs->velocity[1] = 0.0;
        vs->velocity[2] = 0.0;
        
        // Keep vehicle at exactly ground level
        vs->position[2] = 0.0;
        vs->altitude = 0.0;
    }
    else
    {
        // Vehicle is in pre-launch phase - ground support active
        vs->thrust = 0.0;
        
        // Ground support system counteracts gravity
        vs->acceleration[0] = 0.0;
        vs->acceleration[1] = 0.0;
        vs->acceleration[2] = 0.0;
        vs->velocity[0] = 0.0;
        vs->velocity[1] = 0.0;
        vs->velocity[2] = 0.0;
        
        // Keep vehicle at exactly ground level
        vs->position[2] = 0.0;
        vs->altitude = 0.0;
    }

    // Integrate velocity
    for (int i = 0; i < 3; i++)
    {
        vs->velocity[i] += vs->acceleration[i] * dt;
    }

    // Integrate position
    for (int i = 0; i < 3; i++)
    {
        vs->position[i] += vs->velocity[i] * dt;
    }

    // Update altitude
    vs->altitude = vs->position[2];

    // Calculate dynamic pressure and Mach number
    double air_density = 1.225 * exp(-vs->altitude / 8000.0); // Simplified atmosphere
    double velocity_magnitude = sqrt(vs->velocity[0] * vs->velocity[0] +
                                     vs->velocity[1] * vs->velocity[1] +
                                     vs->velocity[2] * vs->velocity[2]);
    vs->dynamic_pressure = 0.5 * air_density * velocity_magnitude * velocity_magnitude;
    vs->mach_number = velocity_magnitude / 343.0; // Speed of sound at sea level

    clock_gettime(CLOCK_REALTIME, &vs->timestamp);
}

/**
 * @brief Calculate guidance commands for current mission phase
 */
static void calculate_guidance_commands(void)
{
    vehicle_state_t *vs = &g_fc_state.vehicle_state;

    switch (g_fc_state.current_phase)
    {
    case PHASE_LIFTOFF:
        // Vertical ascent for first 10 seconds
        g_fc_state.target_velocity[0] = 0.0;
        g_fc_state.target_velocity[1] = 0.0;
        g_fc_state.target_velocity[2] = 50.0; // 50 m/s upward
        break;

    case PHASE_ASCENT:
        // Gravity turn maneuver
        if (vs->altitude > 1000.0)
        {
            double pitch_angle = atan2(vs->altitude - 1000.0, 10000.0); // Gradual turn
            pitch_angle = sls_clamp(pitch_angle, 0.0, M_PI / 3);        // Max 60 degrees

            double target_speed = 200.0 + vs->altitude * 0.01; // Increase with altitude
            g_fc_state.target_velocity[0] = target_speed * sin(pitch_angle);
            g_fc_state.target_velocity[2] = target_speed * cos(pitch_angle);
        }
        break;

    case PHASE_ORBIT_INSERTION:
        // Horizontal acceleration for orbit
        g_fc_state.target_velocity[0] = 7800.0; // Orbital velocity
        g_fc_state.target_velocity[2] = 0.0;    // No vertical component
        break;

    default:
        break;
    }

    g_fc_state.guidance_active = true;
}

/**
 * @brief Update autopilot control system
 */
static void update_autopilot(double dt)
{
    if (!g_fc_state.guidance_active)
    {
        return;
    }

    vehicle_state_t *vs = &g_fc_state.vehicle_state;

    // Simple PID controller for velocity
    for (int axis = 0; axis < 3; axis++)
    {
        double error = g_fc_state.target_velocity[axis] - vs->velocity[axis];

        // Proportional term
        double p_term = g_fc_state.control_gains[0] * error;

        // Integral term
        g_fc_state.integral_error[axis] += error * dt;
        double i_term = g_fc_state.control_gains[1] * g_fc_state.integral_error[axis];

        // Derivative term
        double d_error = (error - g_fc_state.last_error[axis]) / dt;
        double d_term = g_fc_state.control_gains[2] * d_error;

        // Control output (simplified - in reality this would command engine gimbaling)
        double control_output = p_term + i_term + d_term;
        control_output = sls_clamp(control_output, -10.0, 10.0);

        // Apply control (simplified)
        vs->acceleration[axis] += control_output;

        g_fc_state.last_error[axis] = error;
    }
}

/**
 * @brief Simulate atmospheric effects on vehicle
 */
static void simulate_atmospheric_effects(void)
{
    vehicle_state_t *vs = &g_fc_state.vehicle_state;

    if (vs->altitude < 100000.0)
    { // Below 100 km
        // Atmospheric drag
        double drag_coefficient = 0.3;
        double reference_area = 50.0; // m²
        double velocity_magnitude = sqrt(vs->velocity[0] * vs->velocity[0] +
                                         vs->velocity[1] * vs->velocity[1] +
                                         vs->velocity[2] * vs->velocity[2]);

        if (velocity_magnitude > 0.0)
        {
            double air_density = 1.225 * exp(-vs->altitude / 8000.0);
            double drag_force = 0.5 * air_density * velocity_magnitude * velocity_magnitude *
                                drag_coefficient * reference_area;

            // Apply drag acceleration opposite to velocity
            for (int i = 0; i < 3; i++)
            {
                double drag_accel = -(drag_force / vs->mass) * (vs->velocity[i] / velocity_magnitude);
                vs->acceleration[i] += drag_accel;
            }
        }
    }
}

/**
 * @brief Check flight safety constraints
 */
static void check_flight_constraints(void)
{
    vehicle_state_t *vs = &g_fc_state.vehicle_state;

    // Check altitude limits - only error if vehicle is in flight and below ground
    if (vs->altitude < -10.0 && g_fc_state.current_phase >= PHASE_LIFTOFF)
    {
        sls_log(LOG_LEVEL_ERROR, "FCC", "Vehicle below ground level during flight: %.1f m", vs->altitude);
    }
    else if (vs->altitude < -100.0)
    {
        sls_log(LOG_LEVEL_ERROR, "FCC", "Vehicle altitude severely out of bounds: %.1f m", vs->altitude);
    }

    // Check fuel levels
    if (vs->fuel_remaining < 5.0 && g_fc_state.current_phase < PHASE_ORBIT_INSERTION)
    {
        sls_log(LOG_LEVEL_WARNING, "FCC", "Low fuel warning: %.1f%% remaining",
                vs->fuel_remaining);
    }

    // Check dynamic pressure limits
    if (vs->dynamic_pressure > 50000.0)
    { // 50 kPa limit
        sls_log(LOG_LEVEL_WARNING, "FCC", "High dynamic pressure: %.0f Pa",
                vs->dynamic_pressure);
    }

    // Check acceleration limits
    double total_accel = sqrt(vs->acceleration[0] * vs->acceleration[0] +
                              vs->acceleration[1] * vs->acceleration[1] +
                              vs->acceleration[2] * vs->acceleration[2]);
    if (total_accel > 50.0)
    { // 5G limit
        sls_log(LOG_LEVEL_WARNING, "FCC", "High acceleration: %.1f m/s²", total_accel);
    }
}

/**
 * @brief Handle mission phase changes
 */
static void handle_mission_phase_change(mission_phase_t new_phase)
{
    if (new_phase == g_fc_state.current_phase)
    {
        return;
    }

    mission_phase_t old_phase = g_fc_state.current_phase;
    g_fc_state.current_phase = new_phase;

    sls_log(LOG_LEVEL_INFO, "FCC", "Mission phase change: %s -> %s",
            sls_mission_phase_to_string(old_phase),
            sls_mission_phase_to_string(new_phase));

    switch (new_phase)
    {
    case PHASE_IGNITION:
        sls_log(LOG_LEVEL_INFO, "FCC", "Engine ignition sequence initiated");
        break;

    case PHASE_LIFTOFF:
        sls_log(LOG_LEVEL_INFO, "FCC", "LIFTOFF! Vehicle departing launch pad");
        g_fc_state.guidance_active = true;
        break;

    case PHASE_ASCENT:
        sls_log(LOG_LEVEL_INFO, "FCC", "Ascent phase - initiating gravity turn");
        break;

    case PHASE_STAGE_SEPARATION:
        sls_log(LOG_LEVEL_INFO, "FCC", "Stage separation event");
        // Simulate mass reduction
        g_fc_state.vehicle_state.mass *= 0.3; // Upper stage is 30% of original mass
        break;

    case PHASE_ORBIT_INSERTION:
        sls_log(LOG_LEVEL_INFO, "FCC", "Orbit insertion burn initiated");
        break;

    case PHASE_ABORT:
        sls_log(LOG_LEVEL_CRITICAL, "FCC", "MISSION ABORT - Emergency procedures activated");
        g_fc_state.autopilot_enabled = false;
        g_fc_state.guidance_active = false;
        break;

    default:
        break;
    }
}

/**
 * @brief Process incoming status updates and phase changes
 */
static void process_status_updates(void)
{
    // Get current mission phase from main system
    mission_phase_t current_main_phase = sls_get_current_mission_phase();
    
    // Check if phase has changed
    if (current_main_phase != g_fc_state.current_phase)
    {
        handle_mission_phase_change(current_main_phase);
    }
    
    // Process any pending IPC messages
    sls_ipc_process_messages();
}
