#ifndef SLS_LOGGING_H
#define SLS_LOGGING_H

#include <stdio.h>
#include <time.h>

/**
 * @file sls_logging.h
 * @brief Logging system for Space Launch System simulation
 */

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
} log_level_t;

// Log destinations
typedef enum {
    LOG_DEST_CONSOLE = 1,
    LOG_DEST_FILE = 2,
    LOG_DEST_SYSLOG = 4,
    LOG_DEST_ALL = 7
} log_destination_t;

// Logging functions
int sls_logging_init(const char* log_file_path);
void sls_logging_cleanup(void);
void sls_log(log_level_t level, const char* component, const char* format, ...);
void sls_log_raw(log_level_t level, const char* message);

// Configuration
void sls_logging_set_level(log_level_t min_level);
void sls_logging_set_destination(log_destination_t dest);
void sls_logging_enable_timestamps(bool enable);
void sls_logging_enable_colors(bool enable);

// Telemetry logging
void sls_log_telemetry(const char* sensor_name, double value, const char* units);
void sls_log_vehicle_state(double mission_time, double altitude, double velocity);
void sls_log_system_event(const char* event, const char* details);

// Log file management
int sls_logging_rotate_file(void);
size_t sls_logging_get_file_size(void);
void sls_logging_flush(void);

// Convenience macros
#define SLS_LOG_DEBUG(comp, ...) sls_log(LOG_LEVEL_DEBUG, comp, __VA_ARGS__)
#define SLS_LOG_INFO(comp, ...)  sls_log(LOG_LEVEL_INFO, comp, __VA_ARGS__)
#define SLS_LOG_WARN(comp, ...)  sls_log(LOG_LEVEL_WARNING, comp, __VA_ARGS__)
#define SLS_LOG_ERROR(comp, ...) sls_log(LOG_LEVEL_ERROR, comp, __VA_ARGS__)
#define SLS_LOG_CRIT(comp, ...)  sls_log(LOG_LEVEL_CRITICAL, comp, __VA_ARGS__)

#endif // SLS_LOGGING_H
