#ifndef SLS_UTILS_H
#define SLS_UTILS_H

#include "sls_types.h"
#include <time.h>

/**
 * @file sls_utils.h
 * @brief Utility functions for the Space Launch System simulation
 */

// Initialization and cleanup
int sls_utils_init(void);
void sls_utils_cleanup(void);

// Time utilities
double sls_time_to_double(const struct timespec *ts);
void sls_double_to_time(double seconds, struct timespec *ts);
double sls_time_diff(const struct timespec *start, const struct timespec *end);
void sls_time_add_ms(struct timespec *ts, long milliseconds);

// String utilities
void sls_safe_strncpy(char *dest, const char *src, size_t dest_size);
int sls_string_to_subsystem_type(const char *str, subsystem_type_t *type);
const char *sls_subsystem_type_to_string(subsystem_type_t type);
const char *sls_system_state_to_string(system_state_t state);
const char *sls_mission_phase_to_string(mission_phase_t phase);

// Math utilities
double sls_clamp(double value, double min_val, double max_val);
double sls_lerp(double a, double b, double t);
double sls_deg_to_rad(double degrees);
double sls_rad_to_deg(double radians);

// Sensor simulation utilities
double sls_simulate_sensor_noise(double base_value, double noise_amplitude);
bool sls_simulate_sensor_fault(double fault_probability);
double sls_apply_sensor_calibration(double raw_value, double offset, double scale);

// Data validation
bool sls_validate_telemetry_point(const telemetry_point_t *point);
bool sls_validate_sensor_data(const sensor_data_t *sensor);
bool sls_validate_vehicle_state(const vehicle_state_t *state);

// Configuration utilities
int sls_load_config_file(const char *filename);
int sls_get_config_int(const char *key, int default_value);
double sls_get_config_double(const char *key, double default_value);
const char *sls_get_config_string(const char *key, const char *default_value);

// Memory utilities
void *sls_safe_malloc(size_t size);
void *sls_safe_calloc(size_t count, size_t size);
void sls_safe_free(void **ptr);

// Threading utilities
int sls_create_thread(pthread_t *thread, void *(*start_routine)(void *),
                      void *arg, priority_level_t priority);
void sls_set_thread_name(const char *name);

// Subsystem utilities
void *get_subsystem_thread_func(subsystem_type_t type);
const char *get_subsystem_name(subsystem_type_t type);

#endif // SLS_UTILS_H
