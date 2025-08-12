#ifndef SLS_QNX_RMGR_TELEMETRY_H
#define SLS_QNX_RMGR_TELEMETRY_H

#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int running;
    pthread_t thread;
    char device_name[64]; // e.g., /dev/sls_telemetry
} rmgr_telemetry_t;

// Start resource manager and create /dev/sls_telemetry
int rmgr_telemetry_start(rmgr_telemetry_t* ctx, const char* devname);

// Stop resource manager
void rmgr_telemetry_stop(rmgr_telemetry_t* ctx);

// Append one telemetry line (thread-safe). Line should be '\n' terminated.
void rmgr_telemetry_append(const char* line);

#ifdef __cplusplus
}
#endif

#endif // SLS_QNX_RMGR_TELEMETRY_H
