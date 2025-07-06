/**
 * @file sls_utils.c
 * @brief Implementation of utility functions for the Space Launch System simulation
 */

#include "qnx_mock.h"
#include "sls_utils.h"
#include "sls_config.h"
#include "sls_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

// Forward declarations for subsystem thread functions
extern void *flight_control_thread(void *arg);
extern void *engine_control_thread(void *arg);
extern void *telemetry_thread(void *arg);
extern void *environmental_thread(void *arg);
extern void *ground_support_thread(void *arg);
extern void *navigation_thread(void *arg);
extern void *power_thread(void *arg);
extern void *thermal_thread(void *arg);

// Global configuration storage
static bool g_utils_initialized = false;

/**
 * @brief Initialize utility subsystem
 */
int sls_utils_init(void)
{
    if (g_utils_initialized)
    {
        return 0;
    }

    // Initialize random number generator
    srand((unsigned int)time(NULL));

    g_utils_initialized = true;
    return 0;
}

/**
 * @brief Clean up utility subsystem
 */
void sls_utils_cleanup(void)
{
    g_utils_initialized = false;
}

/**
 * @brief Convert timespec to double seconds
 */
double sls_time_to_double(const struct timespec *ts)
{
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

/**
 * @brief Convert double seconds to timespec
 */
void sls_double_to_time(double seconds, struct timespec *ts)
{
    ts->tv_sec = (time_t)seconds;
    ts->tv_nsec = (long)((seconds - (double)ts->tv_sec) * 1000000000.0);
}

/**
 * @brief Calculate time difference in seconds
 */
double sls_time_diff(const struct timespec *start, const struct timespec *end)
{
    return sls_time_to_double(end) - sls_time_to_double(start);
}

/**
 * @brief Add milliseconds to timespec
 */
void sls_time_add_ms(struct timespec *ts, long milliseconds)
{
    ts->tv_nsec += milliseconds * 1000000L;
    if (ts->tv_nsec >= 1000000000L)
    {
        ts->tv_sec += ts->tv_nsec / 1000000000L;
        ts->tv_nsec %= 1000000000L;
    }
}

/**
 * @brief Safe string copy
 */
void sls_safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (dest_size > 0)
    {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
}

/**
 * @brief Convert string to subsystem type
 */
int sls_string_to_subsystem_type(const char *str, subsystem_type_t *type)
{
    if (strcmp(str, "flight_control") == 0)
    {
        *type = SUBSYS_FLIGHT_CONTROL;
    }
    else if (strcmp(str, "engine_control") == 0)
    {
        *type = SUBSYS_ENGINE_CONTROL;
    }
    else if (strcmp(str, "telemetry") == 0)
    {
        *type = SUBSYS_TELEMETRY;
    }
    else if (strcmp(str, "environmental") == 0)
    {
        *type = SUBSYS_ENVIRONMENTAL;
    }
    else if (strcmp(str, "ground_support") == 0)
    {
        *type = SUBSYS_GROUND_SUPPORT;
    }
    else if (strcmp(str, "navigation") == 0)
    {
        *type = SUBSYS_NAVIGATION;
    }
    else if (strcmp(str, "power") == 0)
    {
        *type = SUBSYS_POWER;
    }
    else if (strcmp(str, "thermal") == 0)
    {
        *type = SUBSYS_THERMAL;
    }
    else
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Convert subsystem type to string
 */
const char *sls_subsystem_type_to_string(subsystem_type_t type)
{
    switch (type)
    {
    case SUBSYS_FLIGHT_CONTROL:
        return "Flight Control";
    case SUBSYS_ENGINE_CONTROL:
        return "Engine Control";
    case SUBSYS_TELEMETRY:
        return "Telemetry";
    case SUBSYS_ENVIRONMENTAL:
        return "Environmental";
    case SUBSYS_GROUND_SUPPORT:
        return "Ground Support";
    case SUBSYS_NAVIGATION:
        return "Navigation";
    case SUBSYS_POWER:
        return "Power";
    case SUBSYS_THERMAL:
        return "Thermal";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert system state to string
 */
const char *sls_system_state_to_string(system_state_t state)
{
    switch (state)
    {
    case STATE_OFFLINE:
        return "Offline";
    case STATE_INITIALIZING:
        return "Initializing";
    case STATE_STANDBY:
        return "Standby";
    case STATE_ACTIVE:
        return "Active";
    case STATE_FAULT:
        return "Fault";
    case STATE_EMERGENCY:
        return "Emergency";
    case STATE_SHUTDOWN:
        return "Shutdown";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert mission phase to string
 */
const char *sls_mission_phase_to_string(mission_phase_t phase)
{
    switch (phase)
    {
    case PHASE_PRELAUNCH:
        return "Pre-launch";
    case PHASE_IGNITION:
        return "Ignition";
    case PHASE_LIFTOFF:
        return "Liftoff";
    case PHASE_ASCENT:
        return "Ascent";
    case PHASE_STAGE_SEPARATION:
        return "Stage Separation";
    case PHASE_ORBIT_INSERTION:
        return "Orbit Insertion";
    case PHASE_MISSION_COMPLETE:
        return "Mission Complete";
    case PHASE_ABORT:
        return "Abort";
    default:
        return "Unknown";
    }
}

/**
 * @brief Clamp value between min and max
 */
double sls_clamp(double value, double min_val, double max_val)
{
    if (value < min_val)
        return min_val;
    if (value > max_val)
        return max_val;
    return value;
}

/**
 * @brief Linear interpolation
 */
double sls_lerp(double a, double b, double t)
{
    return a + t * (b - a);
}

/**
 * @brief Convert degrees to radians
 */
double sls_deg_to_rad(double degrees)
{
    return degrees * M_PI / 180.0;
}

/**
 * @brief Convert radians to degrees
 */
double sls_rad_to_deg(double radians)
{
    return radians * 180.0 / M_PI;
}

/**
 * @brief Simulate sensor noise
 */
double sls_simulate_sensor_noise(double base_value, double noise_amplitude)
{
    double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0 * noise_amplitude;
    return base_value + noise;
}

/**
 * @brief Simulate sensor fault based on probability
 */
bool sls_simulate_sensor_fault(double fault_probability)
{
    return ((double)rand() / RAND_MAX) < fault_probability;
}

/**
 * @brief Apply sensor calibration
 */
double sls_apply_sensor_calibration(double raw_value, double offset, double scale)
{
    return (raw_value + offset) * scale;
}

/**
 * @brief Validate telemetry point
 */
bool sls_validate_telemetry_point(const telemetry_point_t *point)
{
    if (!point)
        return false;

    // Check for valid value range
    if (point->value < point->min_value || point->value > point->max_value)
    {
        return false;
    }

    // Check timestamp is reasonable (not too old or in future)
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double age = sls_time_diff(&point->timestamp, &now);
    if (age > 10.0 || age < -1.0)
    { // More than 10 seconds old or 1 second in future
        return false;
    }

    return true;
}

/**
 * @brief Validate sensor data
 */
bool sls_validate_sensor_data(const sensor_data_t *sensor)
{
    if (!sensor)
        return false;

    // Check for reasonable values based on sensor type
    switch (sensor->type)
    {
    case SENSOR_TEMPERATURE:
        return sensor->value >= TEMP_MIN_K && sensor->value <= TEMP_MAX_K;
    case SENSOR_PRESSURE:
        return sensor->value >= PRESSURE_MIN_PA && sensor->value <= PRESSURE_MAX_PA;
    case SENSOR_ACCELERATION:
    case SENSOR_VIBRATION:
        return fabs(sensor->value) <= ACCEL_MAX_G * 9.81; // Convert G to m/s²
    default:
        return true; // Assume valid for unknown types
    }
}

/**
 * @brief Validate vehicle state
 */
bool sls_validate_vehicle_state(const vehicle_state_t *state)
{
    if (!state)
        return false;

    // Check for NaN or infinite values
    for (int i = 0; i < 3; i++)
    {
        if (!isfinite(state->position[i]) || !isfinite(state->velocity[i]) ||
            !isfinite(state->acceleration[i]) || !isfinite(state->angular_velocity[i]))
        {
            return false;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (!isfinite(state->quaternion[i]))
        {
            return false;
        }
    }

    // Check reasonable bounds
    if (state->altitude < -500.0 || state->altitude > 1000000.0)
    { // -500m to 1000km
        return false;
    }

    if (state->fuel_remaining < 0.0 || state->fuel_remaining > 100.0)
    {
        return false;
    }

    return true;
}

/**
 * @brief Safe malloc with error checking
 */
void *sls_safe_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr && size > 0)
    {
        sls_log(LOG_LEVEL_ERROR, "UTILS", "Failed to allocate %zu bytes", size);
    }
    return ptr;
}

/**
 * @brief Safe calloc with error checking
 */
void *sls_safe_calloc(size_t count, size_t size)
{
    void *ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0)
    {
        sls_log(LOG_LEVEL_ERROR, "UTILS", "Failed to allocate %zu * %zu bytes", count, size);
    }
    return ptr;
}

/**
 * @brief Safe free that nullifies pointer
 */
void sls_safe_free(void **ptr)
{
    if (ptr && *ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

/**
 * @brief Create thread with specified priority
 */
int sls_create_thread(pthread_t *thread, void *(*start_routine)(void *),
                      void *arg, priority_level_t priority)
{
    pthread_attr_t attr;
    struct sched_param param;
    int result;

    result = pthread_attr_init(&attr);
    if (result != 0)
        return result;

    result = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (result != 0)
    {
        pthread_attr_destroy(&attr);
        return result;
    }

    param.sched_priority = priority;
    result = pthread_attr_setschedparam(&attr, &param);
    if (result != 0)
    {
        pthread_attr_destroy(&attr);
        return result;
    }

    result = pthread_attr_setstacksize(&attr, QNX_THREAD_STACK_SIZE);
    if (result != 0)
    {
        pthread_attr_destroy(&attr);
        return result;
    }

    result = pthread_create(thread, &attr, start_routine, arg);
    pthread_attr_destroy(&attr);

    return result;
}

/**
 * @brief Set thread name for debugging
 */
void sls_set_thread_name(const char *name)
{
    pthread_setname_np(pthread_self(), name);
}

/**
 * @brief Get subsystem thread function pointer
 */
void *get_subsystem_thread_func(subsystem_type_t type)
{
    switch (type)
    {
    case SUBSYS_FLIGHT_CONTROL:
        return flight_control_thread;
    case SUBSYS_ENGINE_CONTROL:
        return engine_control_thread;
    case SUBSYS_TELEMETRY:
        return telemetry_thread;
    case SUBSYS_ENVIRONMENTAL:
        return environmental_thread;
    case SUBSYS_GROUND_SUPPORT:
        return ground_support_thread;
    case SUBSYS_NAVIGATION:
        return navigation_thread;
    case SUBSYS_POWER:
        return power_thread;
    case SUBSYS_THERMAL:
        return thermal_thread;
    default:
        return NULL;
    }
}

/**
 * @brief Get subsystem name
 */
const char *get_subsystem_name(subsystem_type_t type)
{
    return sls_subsystem_type_to_string(type);
}
