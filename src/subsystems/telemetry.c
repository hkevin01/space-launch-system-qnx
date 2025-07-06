/**
 * @file telemetry.c
 * @brief Telemetry and Communications subsystem for Space Launch System
 *
 * This subsystem handles data collection, formatting, transmission, and logging
 * of all vehicle telemetry data for ground monitoring and mission control.
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

// Telemetry system state
typedef struct
{
    telemetry_point_t telemetry_buffer[MAX_TELEMETRY_POINTS];
    int buffer_count;
    int next_sequence_number;
    FILE *telemetry_log_file;
    bool logging_enabled;
    double mission_time;
    uint32_t packets_sent;
    uint32_t bytes_transmitted;
    struct timespec last_transmission;
} telemetry_state_t;

// Global telemetry state
static telemetry_state_t g_telem_state;
static volatile bool g_telem_shutdown = false;

// Internal function declarations
static void initialize_telemetry(void);
static void process_telemetry_data(double dt);
static void format_telemetry_packet(void);
static void transmit_telemetry(void);
static void log_telemetry_to_file(const telemetry_point_t *point);
static void update_communication_status(void);
static void simulate_transmission_delay(void);

/**
 * @brief Telemetry subsystem thread main function
 */
void *telemetry_thread(void *arg)
{
    subsystem_config_t *config = (subsystem_config_t *)arg;
    sls_set_thread_name("Telemetry");

    sls_log(LOG_LEVEL_INFO, "TELEM", "Telemetry system started (priority %d)",
            config->priority);

    initialize_telemetry();

    struct timespec loop_start, loop_end, sleep_time;
    const long loop_period_ns = (1000000000L / config->update_rate_hz);

    while (!g_telem_shutdown)
    {
        clock_gettime(CLOCK_MONOTONIC, &loop_start);

        // Calculate time delta
        static struct timespec last_update = {0};
        double dt = 0.0;
        if (last_update.tv_sec != 0)
        {
            dt = sls_time_diff(&last_update, &loop_start);
        }
        last_update = loop_start;

        // Update mission time
        g_telem_state.mission_time += dt;

        // Process telemetry data
        process_telemetry_data(dt);

        // Format and transmit telemetry
        format_telemetry_packet();
        transmit_telemetry();

        // Update communication status
        update_communication_status();

        // Send system status
        status_message_t status = {
            .source = SUBSYS_TELEMETRY,
            .state = STATE_ACTIVE,
            .phase = PHASE_PRELAUNCH, // Would be updated from mission controller
            .priority = PRIORITY_NORMAL,
            .error_code = 0};
        snprintf(status.message, sizeof(status.message),
                 "Telemetry active - %u packets sent, %u bytes",
                 g_telem_state.packets_sent, g_telem_state.bytes_transmitted);
        clock_gettime(CLOCK_REALTIME, &status.timestamp);

        // Send status every 10 seconds
        static int status_counter = 0;
        if (++status_counter >= (10 * config->update_rate_hz))
        {
            sls_ipc_send_status(SUBSYS_GROUND_SUPPORT, &status);
            status_counter = 0;
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

    // Cleanup
    if (g_telem_state.telemetry_log_file)
    {
        fclose(g_telem_state.telemetry_log_file);
    }

    sls_log(LOG_LEVEL_INFO, "TELEM", "Telemetry system thread terminated");
    return NULL;
}

/**
 * @brief Initialize telemetry system
 */
static void initialize_telemetry(void)
{
    memset(&g_telem_state, 0, sizeof(g_telem_state));

    g_telem_state.logging_enabled = true;
    g_telem_state.next_sequence_number = 1;
    clock_gettime(CLOCK_REALTIME, &g_telem_state.last_transmission);

    // Open telemetry log file
    g_telem_state.telemetry_log_file = fopen(TELEMETRY_FILE_PATH, "w");
    if (g_telem_state.telemetry_log_file)
    {
        // Write CSV header
        fprintf(g_telem_state.telemetry_log_file,
                "Timestamp,Mission_Time,Telemetry_ID,Name,Type,Value,Units,Quality\n");
        fflush(g_telem_state.telemetry_log_file);
    }
    else
    {
        sls_log(LOG_LEVEL_WARNING, "TELEM", "Failed to open telemetry log file: %s",
                TELEMETRY_FILE_PATH);
    }

    sls_log(LOG_LEVEL_INFO, "TELEM", "Telemetry system initialized");
}

/**
 * @brief Process incoming telemetry data
 */
static void process_telemetry_data(double dt)
{
    // In a real system, this would receive telemetry from all subsystems
    // For simulation, we'll generate some sample telemetry points

    // Generate simulated vehicle telemetry
    telemetry_point_t vehicle_telem[] = {
        {.id = 1001,
         .type = SENSOR_ALTITUDE,
         .value = 1000.0 + g_telem_state.mission_time * 50.0, // Rising altitude
         .min_value = -1000.0,
         .max_value = 1000000.0,
         .valid = true,
         .quality = 100},
        {.id = 1002,
         .type = SENSOR_VELOCITY,
         .value = g_telem_state.mission_time * 10.0, // Increasing velocity
         .min_value = -1000.0,
         .max_value = 10000.0,
         .valid = true,
         .quality = 100},
        {.id = 1003,
         .type = SENSOR_ACCELERATION,
         .value = 9.81 + sls_simulate_sensor_noise(0.0, 0.1),
         .min_value = -50.0,
         .max_value = 50.0,
         .valid = true,
         .quality = 100}};

    // Copy telemetry names and units
    strcpy(vehicle_telem[0].name, "Vehicle_Altitude");
    strcpy(vehicle_telem[0].units, "m");
    strcpy(vehicle_telem[1].name, "Vehicle_Velocity");
    strcpy(vehicle_telem[1].units, "m/s");
    strcpy(vehicle_telem[2].name, "Vehicle_Acceleration");
    strcpy(vehicle_telem[2].units, "m/s²");

    // Add timestamps
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    for (int i = 0; i < 3; i++)
    {
        vehicle_telem[i].timestamp = now;
    }

    // Store in buffer if space available
    for (int i = 0; i < 3 && g_telem_state.buffer_count < MAX_TELEMETRY_POINTS; i++)
    {
        g_telem_state.telemetry_buffer[g_telem_state.buffer_count] = vehicle_telem[i];
        g_telem_state.buffer_count++;

        // Log to file
        if (g_telem_state.logging_enabled)
        {
            log_telemetry_to_file(&vehicle_telem[i]);
        }
    }
}

/**
 * @brief Format telemetry packet for transmission
 */
static void format_telemetry_packet(void)
{
    if (g_telem_state.buffer_count == 0)
    {
        return;
    }

    // In a real system, this would format data according to a specific protocol
    // For simulation, we'll just log the formatting process

    static int format_counter = 0;
    if (++format_counter % 100 == 0)
    { // Log every 100 packets
        sls_log(LOG_LEVEL_DEBUG, "TELEM", "Formatted telemetry packet with %d points",
                g_telem_state.buffer_count);
    }
}

/**
 * @brief Transmit telemetry data
 */
static void transmit_telemetry(void)
{
    if (g_telem_state.buffer_count == 0)
    {
        return;
    }

    // Simulate transmission delay
    simulate_transmission_delay();

    // Calculate packet size (simplified)
    size_t packet_size = sizeof(telemetry_point_t) * g_telem_state.buffer_count + 64; // Header

    // Update statistics
    g_telem_state.packets_sent++;
    g_telem_state.bytes_transmitted += packet_size;
    clock_gettime(CLOCK_REALTIME, &g_telem_state.last_transmission);

    // Log telemetry transmission
    static int tx_counter = 0;
    if (++tx_counter % 50 == 0)
    { // Log every 50 transmissions
        sls_log(LOG_LEVEL_DEBUG, "TELEM", "Transmitted packet #%u (%zu bytes, %d points)",
                g_telem_state.packets_sent, packet_size, g_telem_state.buffer_count);
    }

    // Clear buffer after transmission
    g_telem_state.buffer_count = 0;
}

/**
 * @brief Log telemetry point to file
 */
static void log_telemetry_to_file(const telemetry_point_t *point)
{
    if (!g_telem_state.telemetry_log_file || !point)
    {
        return;
    }

    // Format timestamp
    char timestamp_str[32];
    struct tm *tm_info = localtime(&point->timestamp.tv_sec);
    snprintf(timestamp_str, sizeof(timestamp_str), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             point->timestamp.tv_nsec / 1000000);

    // Write CSV record
    fprintf(g_telem_state.telemetry_log_file,
            "%s,%.3f,%u,%s,%d,%.6f,%s,%u\n",
            timestamp_str, g_telem_state.mission_time, point->id, point->name,
            (int)point->type, point->value, point->units, point->quality);

    // Flush periodically
    static int flush_counter = 0;
    if (++flush_counter % 10 == 0)
    {
        fflush(g_telem_state.telemetry_log_file);
    }
}

/**
 * @brief Update communication status
 */
static void update_communication_status(void)
{
    // Check for communication health
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    double time_since_tx = sls_time_diff(&g_telem_state.last_transmission, &now);

    // Generate communication telemetry
    telemetry_point_t comm_telem[] = {
        {.id = 3001,
         .type = SENSOR_FLOW_RATE,
         .value = (double)g_telem_state.packets_sent,
         .min_value = 0.0,
         .max_value = 1000000.0,
         .valid = true,
         .quality = 100},
        {.id = 3002,
         .type = SENSOR_FLOW_RATE,
         .value = (double)g_telem_state.bytes_transmitted,
         .min_value = 0.0,
         .max_value = 1000000000.0,
         .valid = true,
         .quality = 100},
        {.id = 3003,
         .type = SENSOR_TEMPERATURE,
         .value = time_since_tx,
         .min_value = 0.0,
         .max_value = 60.0,
         .valid = true,
         .quality = time_since_tx < 10.0 ? 100 : 50}};

    strcpy(comm_telem[0].name, "Comm_PacketsSent");
    strcpy(comm_telem[0].units, "count");
    strcpy(comm_telem[1].name, "Comm_BytesTransmitted");
    strcpy(comm_telem[1].units, "bytes");
    strcpy(comm_telem[2].name, "Comm_TimeSinceLastTx");
    strcpy(comm_telem[2].units, "s");

    clock_gettime(CLOCK_REALTIME, &now);
    for (int i = 0; i < 3; i++)
    {
        comm_telem[i].timestamp = now;
    }

    // Add to buffer if space available
    for (int i = 0; i < 3 && g_telem_state.buffer_count < MAX_TELEMETRY_POINTS; i++)
    {
        g_telem_state.telemetry_buffer[g_telem_state.buffer_count] = comm_telem[i];
        g_telem_state.buffer_count++;

        if (g_telem_state.logging_enabled)
        {
            log_telemetry_to_file(&comm_telem[i]);
        }
    }
}

/**
 * @brief Simulate transmission delay
 */
static void simulate_transmission_delay(void)
{
    // Simulate realistic transmission delay (microseconds to milliseconds)
    usleep(100 + (rand() % 1000)); // 0.1-1.1 ms delay
}
