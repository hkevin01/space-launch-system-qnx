// QNX-only main simulation demonstrating QNX IPC, pulses, resource manager, and slog2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "ipc.h"
#include "../common/slog.h"
#include "rmgr_telemetry.h"

static volatile int g_mission_go = 0;
static volatile int g_throttle = 0;   // 0..100
static volatile int g_abort_req = 0;

static double g_mission_time = 0.0;   // seconds
static double g_altitude = 0.0;       // meters
static double g_velocity = 0.0;       // m/s

static void step_sim(double dt) {
    if (g_abort_req) {
        // simple shutdown behavior
        if (g_throttle > 0) g_throttle -= (int)(50 * dt); // ramp down quickly
        if (g_throttle < 0) g_throttle = 0;
        g_mission_go = 0;
    }
    if (g_mission_go && g_throttle > 0) {
        double thrust_factor = g_throttle / 100.0; // simplistic dynamics
        g_velocity += 5.0 * thrust_factor * dt;
        g_altitude += g_velocity * dt;
    } else {
        // gravity pull-down (very simplified)
        g_velocity -= 1.0 * dt;
        if (g_velocity < 0 && g_altitude <= 0) {
            g_velocity = 0; g_altitude = 0;
        } else {
            g_altitude += g_velocity * dt;
            if (g_altitude < 0) g_altitude = 0;
        }
    }
    g_mission_time += dt;
}

static void append_telem_line(void) {
    char buf[256];
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    // timestamp, altitude, velocity, throttle, status
    snprintf(buf, sizeof(buf), "%ld.%03ld,alt=%.2f,vel=%.2f,thr=%d,go=%d\n",
             ts.tv_sec, ts.tv_nsec/1000000, g_altitude, g_velocity, g_throttle, g_mission_go);
    rmgr_telemetry_append(buf);
}

int main(void) {
    if (sls_slog_init() != 0) {
        fprintf(stderr, "slog2 init failed\n");
    }
    SLOGI("MAIN", "SLS QNX demo starting");

    rmgr_telemetry_t rctx; memset(&rctx, 0, sizeof(rctx));
    if (rmgr_telemetry_start(&rctx, "/dev/sls_telemetry") != 0) {
        SLOGW("RMGR", "Failed to start telemetry resource manager");
    } else {
        SLOGI("RMGR", "Telemetry available at /dev/sls_telemetry");
    }

    ipc_server_t server;
    if (ipc_server_start(&server, "sls_fcc", &g_mission_go, &g_throttle, &g_abort_req, 70) != 0) {
        SLOGE("IPC", "Failed to start IPC server");
        return 1;
    }
    timer_t tick_timer = 0; // optional, simulation uses nanosleep here as well
    timer_pulse_start(server.chid, 100, PULSE_TICK, 0, &tick_timer);

    const double dt = 0.1; // 100ms
    while (!g_abort_req || g_altitude > 0.0 || g_velocity > 0.0) {
        step_sim(dt);
        append_telem_line();
        if (g_mission_time > 36000) break; // safety stop
        struct timespec req = { .tv_sec = 0, .tv_nsec = 100000000L };
        nanosleep(&req, NULL);
    }

    SLOGI("MAIN", "SLS QNX demo shutting down");
    ipc_server_stop(&server);
    rmgr_telemetry_stop(&rctx);
    return 0;
}
