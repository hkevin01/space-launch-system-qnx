/**
 * @file test_main.c
 * @brief Unit tests for Space Launch System simulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "../src/common/sls_types.h"
#include "../src/common/sls_utils.h"
#include "../src/common/sls_logging.h"

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test)                   \
    do                                   \
    {                                    \
        printf("Running " #test "... "); \
        tests_run++;                     \
        if (test())                      \
        {                                \
            printf("PASSED\n");          \
            tests_passed++;              \
        }                                \
        else                             \
        {                                \
            printf("FAILED\n");          \
        }                                \
    } while (0)

// Test utility functions
int test_time_utilities()
{
    struct timespec ts1 = {1000, 500000000}; // 1000.5 seconds
    struct timespec ts2 = {1002, 750000000}; // 1002.75 seconds

    double d1 = sls_time_to_double(&ts1);
    double d2 = sls_time_to_double(&ts2);

    if (d1 != 1000.5)
        return 0;
    if (d2 != 1002.75)
        return 0;

    double diff = sls_time_diff(&ts1, &ts2);
    if (diff < 2.24 || diff > 2.26)
        return 0; // Should be 2.25

    return 1;
}

// Test data validation
int test_telemetry_validation()
{
    telemetry_point_t valid_point = {
        .id = 1001,
        .value = 50.0,
        .min_value = 0.0,
        .max_value = 100.0,
        .valid = true,
        .quality = 100};
    clock_gettime(CLOCK_REALTIME, &valid_point.timestamp);

    if (!sls_validate_telemetry_point(&valid_point))
        return 0;

    // Test invalid range
    telemetry_point_t invalid_point = valid_point;
    invalid_point.value = 150.0; // Outside range

    if (sls_validate_telemetry_point(&invalid_point))
        return 0;

    return 1;
}

// Test math utilities
int test_math_utilities()
{
    // Test clamp
    if (sls_clamp(5.0, 0.0, 10.0) != 5.0)
        return 0;
    if (sls_clamp(-5.0, 0.0, 10.0) != 0.0)
        return 0;
    if (sls_clamp(15.0, 0.0, 10.0) != 10.0)
        return 0;

    // Test interpolation
    if (sls_lerp(0.0, 10.0, 0.5) != 5.0)
        return 0;
    if (sls_lerp(0.0, 10.0, 0.0) != 0.0)
        return 0;
    if (sls_lerp(0.0, 10.0, 1.0) != 10.0)
        return 0;

    // Test angle conversion
    double rad = sls_deg_to_rad(90.0);
    if (rad < 1.57 || rad > 1.58)
        return 0; // Should be π/2

    double deg = sls_rad_to_deg(3.14159);
    if (deg < 179.9 || deg > 180.1)
        return 0; // Should be ~180

    return 1;
}

// Test string utilities
int test_string_utilities()
{
    char buffer[20];
    sls_safe_strncpy(buffer, "Hello, World!", sizeof(buffer));
    if (strcmp(buffer, "Hello, World!") != 0)
        return 0;

    // Test truncation
    sls_safe_strncpy(buffer, "This is a very long string", 10);
    if (strlen(buffer) >= 10)
        return 0;

    // Test subsystem string conversion
    if (strcmp(sls_subsystem_type_to_string(SUBSYS_FLIGHT_CONTROL), "Flight Control") != 0)
        return 0;
    if (strcmp(sls_system_state_to_string(STATE_ACTIVE), "Active") != 0)
        return 0;

    return 1;
}

// Test vehicle state validation
int test_vehicle_state_validation()
{
    vehicle_state_t valid_state = {
        .position = {0.0, 0.0, 1000.0},
        .velocity = {100.0, 0.0, 50.0},
        .acceleration = {0.0, 0.0, -9.81},
        .quaternion = {1.0, 0.0, 0.0, 0.0},
        .angular_velocity = {0.0, 0.0, 0.0},
        .altitude = 1000.0,
        .fuel_remaining = 75.0,
        .mission_time = 120.0};

    if (!sls_validate_vehicle_state(&valid_state))
        return 0;

    // Test invalid altitude
    vehicle_state_t invalid_state = valid_state;
    invalid_state.altitude = -1000.0; // Below reasonable limit

    if (sls_validate_vehicle_state(&invalid_state))
        return 0;

    // Test invalid fuel
    invalid_state = valid_state;
    invalid_state.fuel_remaining = 150.0; // Above 100%

    if (sls_validate_vehicle_state(&invalid_state))
        return 0;

    return 1;
}

// Test logging system
int test_logging_system()
{
    // Initialize logging to a test file
    if (sls_logging_init("/tmp/test_log.txt") != 0)
        return 0;

    // Test logging at different levels
    sls_log(LOG_LEVEL_INFO, "TEST", "Test log message");
    sls_log(LOG_LEVEL_WARNING, "TEST", "Test warning message");
    sls_log(LOG_LEVEL_ERROR, "TEST", "Test error message");

    // Test telemetry logging
    sls_log_telemetry("TestSensor", 42.5, "units");
    sls_log_vehicle_state(120.0, 1000.0, 100.0);
    sls_log_system_event("TestEvent", "Test details");

    sls_logging_cleanup();

    // Check if file was created
    FILE *test_file = fopen("/tmp/test_log.txt", "r");
    if (!test_file)
        return 0;

    fclose(test_file);
    unlink("/tmp/test_log.txt"); // Clean up

    return 1;
}

int main()
{
    printf("QNX Space Launch System - Unit Tests\n");
    printf("====================================\n\n");

    // Initialize utils for testing
    sls_utils_init();

    // Run tests
    RUN_TEST(test_time_utilities);
    RUN_TEST(test_telemetry_validation);
    RUN_TEST(test_math_utilities);
    RUN_TEST(test_string_utilities);
    RUN_TEST(test_vehicle_state_validation);
    RUN_TEST(test_logging_system);

    // Cleanup
    sls_utils_cleanup();

    // Print results
    printf("\n====================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run)
    {
        printf("All tests PASSED!\n");
        return 0;
    }
    else
    {
        printf("Some tests FAILED!\n");
        return 1;
    }
}
