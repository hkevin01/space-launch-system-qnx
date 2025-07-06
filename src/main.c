/**
 * @file main.c
 * @brief Main entry point for QNX Space Launch System Simulation
 * 
 * This application simulates a real-time space launch system using QNX Neutrino RTOS.
 * It coordinates multiple subsystems, handles real-time telemetry, and provides
 * a comprehensive simulation environment for space launch operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>

#include "common/sls_types.h"
#include "common/sls_config.h"
#include "common/sls_utils.h"
#include "common/sls_ipc.h"
#include "common/sls_logging.h"

// Global system state
static volatile bool g_shutdown_requested = false;
static volatile mission_phase_t g_current_phase = PHASE_PRELAUNCH;
static volatile system_state_t g_system_state = STATE_INITIALIZING;
static double g_mission_time = -7200.0; // Start at T-2 hours

// Thread handles for subsystems
static pthread_t subsystem_threads[MAX_SUBSYSTEMS];
static int active_subsystems = 0;

// Forward declarations
static void signal_handler(int signum);
static int initialize_system(void);
static int start_subsystems(void);
static int main_control_loop(void);
static void shutdown_system(void);
static void update_mission_phase(void);
static void* subsystem_monitor_thread(void* arg);

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            printf("\n[MAIN] Shutdown signal received (%d). Initiating graceful shutdown...\n", signum);
            g_shutdown_requested = true;
            break;
        default:
            break;
    }
}

/**
 * @brief Initialize the QNX system and core components
 */
static int initialize_system(void) {
    printf("[MAIN] Initializing QNX Space Launch System Simulation...\n");
    
    // Initialize logging system
    if (sls_logging_init(LOG_FILE_PATH) != 0) {
        fprintf(stderr, "[MAIN] Failed to initialize logging system\n");
        return -1;
    }
    
    sls_log(LOG_LEVEL_INFO, "MAIN", "System initialization started");
    
    // Initialize IPC system
    if (sls_ipc_init() != 0) {
        sls_log(LOG_LEVEL_ERROR, "MAIN", "Failed to initialize IPC system");
        return -1;
    }
    
    // Initialize utils and timing
    if (sls_utils_init() != 0) {
        sls_log(LOG_LEVEL_ERROR, "MAIN", "Failed to initialize utilities");
        return -1;
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    sls_log(LOG_LEVEL_INFO, "MAIN", "Core system initialization complete");
    return 0;
}

/**
 * @brief Start all subsystem threads
 */
static int start_subsystems(void) {
    sls_log(LOG_LEVEL_INFO, "MAIN", "Starting subsystem threads...");
    
    // Create subsystem configuration
    subsystem_config_t configs[] = DEFAULT_SUBSYSTEM_CONFIGS;
    int num_configs = sizeof(configs) / sizeof(configs[0]);
    
    for (int i = 0; i < num_configs && i < MAX_SUBSYSTEMS; i++) {
        pthread_attr_t attr;
        struct sched_param param;
        
        // Initialize thread attributes
        if (pthread_attr_init(&attr) != 0) {
            sls_log(LOG_LEVEL_ERROR, "MAIN", "Failed to initialize thread attributes for subsystem %d", i);
            continue;
        }
        
        // Set scheduling policy and priority
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        param.sched_priority = configs[i].priority;
        pthread_attr_setschedparam(&attr, &param);
        
        // Set stack size
        pthread_attr_setstacksize(&attr, QNX_THREAD_STACK_SIZE);
        
        // Create subsystem thread
        if (pthread_create(&subsystem_threads[i], &attr, 
                          get_subsystem_thread_func(configs[i].type), 
                          &configs[i]) != 0) {
            sls_log(LOG_LEVEL_ERROR, "MAIN", "Failed to create thread for subsystem %s", 
                   configs[i].name);
            pthread_attr_destroy(&attr);
            continue;
        }
        
        pthread_attr_destroy(&attr);
        active_subsystems++;
        
        sls_log(LOG_LEVEL_INFO, "MAIN", "Started subsystem: %s (priority %d)", 
               configs[i].name, configs[i].priority);
    }
    
    // Start subsystem monitor thread
    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, subsystem_monitor_thread, NULL) != 0) {
        sls_log(LOG_LEVEL_ERROR, "MAIN", "Failed to create subsystem monitor thread");
        return -1;
    }
    
    sls_log(LOG_LEVEL_INFO, "MAIN", "All subsystems started successfully (%d active)", 
           active_subsystems);
    return 0;
}

/**
 * @brief Update mission phase based on current mission time
 */
static void update_mission_phase(void) {
    static mission_phase_t last_phase = PHASE_UNKNOWN;
    phase_config_t phases[] = DEFAULT_MISSION_PHASES;
    int num_phases = sizeof(phases) / sizeof(phases[0]);
    
    mission_phase_t new_phase = g_current_phase;
    
    for (int i = 0; i < num_phases; i++) {
        if (g_mission_time >= phases[i].start_time && 
            g_mission_time < (phases[i].start_time + phases[i].duration)) {
            new_phase = phases[i].phase;
            break;
        }
    }
    
    if (new_phase != last_phase) {
        g_current_phase = new_phase;
        sls_log(LOG_LEVEL_INFO, "MAIN", "Mission phase changed to: %d at T%+.1f", 
               new_phase, g_mission_time);
        
        // Broadcast phase change to all subsystems
        status_message_t phase_msg = {
            .source = SUBSYS_FLIGHT_CONTROL,
            .state = g_system_state,
            .phase = new_phase,
            .priority = PRIORITY_HIGH,
            .error_code = 0
        };
        snprintf(phase_msg.message, sizeof(phase_msg.message), 
                "Mission phase changed to %d", new_phase);
        clock_gettime(CLOCK_REALTIME, &phase_msg.timestamp);
        
        sls_ipc_broadcast_status(&phase_msg);
        last_phase = new_phase;
    }
}

/**
 * @brief Main control loop
 */
static int main_control_loop(void) {
    sls_log(LOG_LEVEL_INFO, "MAIN", "Entering main control loop");
    g_system_state = STATE_ACTIVE;
    
    struct timespec loop_start, loop_end, sleep_time;
    const long loop_period_ns = MAIN_LOOP_PERIOD_MS * 1000000L; // Convert to nanoseconds
    
    while (!g_shutdown_requested) {
        clock_gettime(CLOCK_MONOTONIC, &loop_start);
        
        // Update mission time (real-time simulation)
        g_mission_time += (double)MAIN_LOOP_PERIOD_MS / 1000.0;
        
        // Update mission phase
        update_mission_phase();
        
        // Process any pending commands or messages
        sls_ipc_process_messages();
        
        // Check for system faults or emergency conditions
        if (g_current_phase == PHASE_ABORT) {
            sls_log(LOG_LEVEL_CRITICAL, "MAIN", "Mission abort detected, initiating emergency procedures");
            g_system_state = STATE_EMERGENCY;
            // Emergency shutdown procedures would go here
        }
        
        // Calculate sleep time to maintain loop period
        clock_gettime(CLOCK_MONOTONIC, &loop_end);
        long elapsed_ns = (loop_end.tv_sec - loop_start.tv_sec) * 1000000000L + 
                         (loop_end.tv_nsec - loop_start.tv_nsec);
        
        if (elapsed_ns < loop_period_ns) {
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = loop_period_ns - elapsed_ns;
            nanosleep(&sleep_time, NULL);
        } else {
            sls_log(LOG_LEVEL_WARNING, "MAIN", "Main loop overrun by %ld ns", 
                   elapsed_ns - loop_period_ns);
        }
    }
    
    sls_log(LOG_LEVEL_INFO, "MAIN", "Main control loop terminated");
    return 0;
}

/**
 * @brief Monitor subsystem health and status
 */
static void* subsystem_monitor_thread(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    sls_log(LOG_LEVEL_INFO, "MONITOR", "Subsystem monitor thread started");
    
    while (!g_shutdown_requested) {
        // Check subsystem health
        for (int i = 0; i < active_subsystems; i++) {
            // This would check heartbeat, memory usage, CPU usage, etc.
            // For now, just a placeholder
        }
        
        // Sleep for monitoring interval
        sleep(5); // Check every 5 seconds
    }
    
    sls_log(LOG_LEVEL_INFO, "MONITOR", "Subsystem monitor thread terminated");
    return NULL;
}

/**
 * @brief Shutdown system gracefully
 */
static void shutdown_system(void) {
    sls_log(LOG_LEVEL_INFO, "MAIN", "Initiating system shutdown...");
    g_system_state = STATE_SHUTDOWN;
    
    // Signal all subsystems to shutdown
    status_message_t shutdown_msg = {
        .source = SUBSYS_FLIGHT_CONTROL,
        .state = STATE_SHUTDOWN,
        .phase = g_current_phase,
        .priority = PRIORITY_CRITICAL,
        .error_code = 0
    };
    strcpy(shutdown_msg.message, "System shutdown initiated");
    clock_gettime(CLOCK_REALTIME, &shutdown_msg.timestamp);
    
    sls_ipc_broadcast_status(&shutdown_msg);
    
    // Wait for subsystem threads to terminate
    for (int i = 0; i < active_subsystems; i++) {
        if (pthread_join(subsystem_threads[i], NULL) != 0) {
            sls_log(LOG_LEVEL_WARNING, "MAIN", "Failed to join subsystem thread %d", i);
        }
    }
    
    // Cleanup systems
    sls_ipc_cleanup();
    sls_utils_cleanup();
    sls_logging_cleanup();
    
    printf("[MAIN] System shutdown complete.\n");
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    int exit_code = EXIT_SUCCESS;
    
    printf("QNX Space Launch System Simulation v1.0\n");
    printf("========================================\n\n");
    
    // Parse command line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -h, --help     Show this help message\n");
            printf("  --version      Show version information\n");
            printf("  --config FILE  Use custom configuration file\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[1], "--version") == 0) {
            printf("Version: 1.0.0\n");
            printf("Build: %s %s\n", __DATE__, __TIME__);
            return EXIT_SUCCESS;
        }
    }
    
    // Initialize system
    if (initialize_system() != 0) {
        fprintf(stderr, "Failed to initialize system\n");
        return EXIT_FAILURE;
    }
    
    // Start subsystems
    if (start_subsystems() != 0) {
        fprintf(stderr, "Failed to start subsystems\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    
    // Run main control loop
    if (main_control_loop() != 0) {
        fprintf(stderr, "Error in main control loop\n");
        exit_code = EXIT_FAILURE;
    }
    
cleanup:
    shutdown_system();
    return exit_code;
}
