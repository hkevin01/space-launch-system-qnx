/**
 * @file sls_logging.c
 * @brief Implementation of logging system for Space Launch System
 */

#include "sls_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BRIGHT_RED "\033[91m"

// Internal configuration
static FILE *g_log_file = NULL;
static char g_log_file_path[256] = {0};
static log_level_t g_min_log_level = LOG_LEVEL_INFO;
static log_destination_t g_log_destination = LOG_DEST_CONSOLE | LOG_DEST_FILE;
static bool g_timestamps_enabled = true;
static bool g_colors_enabled = true;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_logging_initialized = false;

// Internal functions
static const char *log_level_to_string(log_level_t level);
static const char *log_level_to_color(log_level_t level);
static void format_timestamp(char *buffer, size_t buffer_size);
static void write_log_entry(log_level_t level, const char *component, const char *message);

/**
 * @brief Initialize logging system
 */
int sls_logging_init(const char *log_file_path)
{
    pthread_mutex_lock(&g_log_mutex);

    if (g_logging_initialized)
    {
        pthread_mutex_unlock(&g_log_mutex);
        return 0;
    }

    // Copy log file path
    if (log_file_path)
    {
        strncpy(g_log_file_path, log_file_path, sizeof(g_log_file_path) - 1);
        g_log_file_path[sizeof(g_log_file_path) - 1] = '\0';

        // Open log file
        g_log_file = fopen(g_log_file_path, "a");
        if (!g_log_file)
        {
            fprintf(stderr, "Failed to open log file: %s\n", g_log_file_path);
            pthread_mutex_unlock(&g_log_mutex);
            return -1;
        }
    }

    // Disable colors if output is not a terminal
    if (!isatty(STDOUT_FILENO))
    {
        g_colors_enabled = false;
    }

    g_logging_initialized = true;
    pthread_mutex_unlock(&g_log_mutex);

    // Log initialization message
    sls_log(LOG_LEVEL_INFO, "LOGGING", "Logging system initialized");
    if (log_file_path)
    {
        sls_log(LOG_LEVEL_INFO, "LOGGING", "Log file: %s", log_file_path);
    }

    return 0;
}

/**
 * @brief Cleanup logging system
 */
void sls_logging_cleanup(void)
{
    pthread_mutex_lock(&g_log_mutex);

    if (!g_logging_initialized)
    {
        pthread_mutex_unlock(&g_log_mutex);
        return;
    }

    sls_log(LOG_LEVEL_INFO, "LOGGING", "Shutting down logging system");

    if (g_log_file)
    {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    g_logging_initialized = false;
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Main logging function
 */
void sls_log(log_level_t level, const char *component, const char *format, ...)
{
    if (!g_logging_initialized || level < g_min_log_level)
    {
        return;
    }

    // Format message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    write_log_entry(level, component, message);
}

/**
 * @brief Log raw message without formatting
 */
void sls_log_raw(log_level_t level, const char *message)
{
    if (!g_logging_initialized || level < g_min_log_level || !message)
    {
        return;
    }

    write_log_entry(level, "RAW", message);
}

/**
 * @brief Set minimum log level
 */
void sls_logging_set_level(log_level_t min_level)
{
    pthread_mutex_lock(&g_log_mutex);
    g_min_log_level = min_level;
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Set log destination
 */
void sls_logging_set_destination(log_destination_t dest)
{
    pthread_mutex_lock(&g_log_mutex);
    g_log_destination = dest;
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Enable/disable timestamps
 */
void sls_logging_enable_timestamps(bool enable)
{
    pthread_mutex_lock(&g_log_mutex);
    g_timestamps_enabled = enable;
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Enable/disable colors
 */
void sls_logging_enable_colors(bool enable)
{
    pthread_mutex_lock(&g_log_mutex);
    g_colors_enabled = enable;
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Log telemetry data
 */
void sls_log_telemetry(const char *sensor_name, double value, const char *units)
{
    if (!sensor_name || !units)
    {
        return;
    }

    sls_log(LOG_LEVEL_DEBUG, "TELEMETRY", "%s: %.3f %s", sensor_name, value, units);
}

/**
 * @brief Log vehicle state
 */
void sls_log_vehicle_state(double mission_time, double altitude, double velocity)
{
    sls_log(LOG_LEVEL_INFO, "VEHICLE", "T%+.1f Alt=%.0fm Vel=%.1fm/s",
            mission_time, altitude, velocity);
}

/**
 * @brief Log system event
 */
void sls_log_system_event(const char *event, const char *details)
{
    if (!event)
    {
        return;
    }

    if (details)
    {
        sls_log(LOG_LEVEL_INFO, "EVENT", "%s: %s", event, details);
    }
    else
    {
        sls_log(LOG_LEVEL_INFO, "EVENT", "%s", event);
    }
}

/**
 * @brief Rotate log file
 */
int sls_logging_rotate_file(void)
{
    pthread_mutex_lock(&g_log_mutex);

    if (!g_log_file || strlen(g_log_file_path) == 0)
    {
        pthread_mutex_unlock(&g_log_mutex);
        return -1;
    }

    // Close current file
    fclose(g_log_file);

    // Create backup filename with timestamp
    char backup_path[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    snprintf(backup_path, sizeof(backup_path), "%s.%04d%02d%02d_%02d%02d%02d",
             g_log_file_path,
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    // Rename current file to backup
    if (rename(g_log_file_path, backup_path) != 0)
    {
        pthread_mutex_unlock(&g_log_mutex);
        return -1;
    }

    // Open new log file
    g_log_file = fopen(g_log_file_path, "a");
    if (!g_log_file)
    {
        pthread_mutex_unlock(&g_log_mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_log_mutex);

    sls_log(LOG_LEVEL_INFO, "LOGGING", "Log file rotated. Backup: %s", backup_path);
    return 0;
}

/**
 * @brief Get log file size
 */
size_t sls_logging_get_file_size(void)
{
    if (!g_log_file)
    {
        return 0;
    }

    struct stat st;
    if (stat(g_log_file_path, &st) == 0)
    {
        return st.st_size;
    }

    return 0;
}

/**
 * @brief Flush log buffers
 */
void sls_logging_flush(void)
{
    pthread_mutex_lock(&g_log_mutex);

    if (g_log_file)
    {
        fflush(g_log_file);
    }
    fflush(stdout);
    fflush(stderr);

    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * @brief Convert log level to string
 */
static const char *log_level_to_string(log_level_t level)
{
    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO ";
    case LOG_LEVEL_WARNING:
        return "WARN ";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_CRITICAL:
        return "CRIT ";
    default:
        return "UNKN ";
    }
}

/**
 * @brief Get color code for log level
 */
static const char *log_level_to_color(log_level_t level)
{
    if (!g_colors_enabled)
    {
        return "";
    }

    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        return COLOR_CYAN;
    case LOG_LEVEL_INFO:
        return COLOR_WHITE;
    case LOG_LEVEL_WARNING:
        return COLOR_YELLOW;
    case LOG_LEVEL_ERROR:
        return COLOR_RED;
    case LOG_LEVEL_CRITICAL:
        return COLOR_BRIGHT_RED;
    default:
        return COLOR_WHITE;
    }
}

/**
 * @brief Format timestamp string
 */
static void format_timestamp(char *buffer, size_t buffer_size)
{
    if (!g_timestamps_enabled)
    {
        buffer[0] = '\0';
        return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm *tm_info = localtime(&ts.tv_sec);
    snprintf(buffer, buffer_size, "%02d:%02d:%02d.%03ld",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             ts.tv_nsec / 1000000);
}

/**
 * @brief Write log entry to configured destinations
 */
static void write_log_entry(log_level_t level, const char *component, const char *message)
{
    pthread_mutex_lock(&g_log_mutex);

    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp));

    // Format complete log line
    char log_line[2048];
    if (g_timestamps_enabled)
    {
        snprintf(log_line, sizeof(log_line), "[%s] %s %-12s: %s",
                 timestamp, log_level_to_string(level), component, message);
    }
    else
    {
        snprintf(log_line, sizeof(log_line), "%s %-12s: %s",
                 log_level_to_string(level), component, message);
    }

    // Write to console
    if (g_log_destination & LOG_DEST_CONSOLE)
    {
        const char *color = log_level_to_color(level);
        const char *reset = g_colors_enabled ? COLOR_RESET : "";

        FILE *output = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fprintf(output, "%s%s%s\n", color, log_line, reset);
    }

    // Write to file
    if ((g_log_destination & LOG_DEST_FILE) && g_log_file)
    {
        fprintf(g_log_file, "%s\n", log_line);
        fflush(g_log_file);
    }

    pthread_mutex_unlock(&g_log_mutex);
}
