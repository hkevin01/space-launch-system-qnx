#ifndef SLS_QNX_IPC_H
#define SLS_QNX_IPC_H

#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple command protocol
typedef enum {
    CMD_STATUS = 1,
    CMD_GO = 2,
    CMD_NOGO = 3,
    CMD_ABORT = 4,
    CMD_SET_THROTTLE = 5,
    PULSE_TICK = 100
} cmd_t;

typedef struct {
    int32_t type;   // cmd_t
    int32_t value;  // throttle percent or extra data
} sim_msg_t;

typedef struct {
    int32_t ok;         // 0/1
    int32_t mission_go; // 0/1
    int32_t throttle;   // 0-100
} sim_reply_t;

// Server context
typedef struct {
    name_attach_t* attach;   // name_attach handle for clients
    int chid;                // Channel ID
    pthread_t thread;        // Receiver thread
    int prio;                // Priority for receiver thread
    // Application state pointers
    volatile int* mission_go;
    volatile int* throttle;
    volatile int* abort_req;
} ipc_server_t;

// Start a message-passing server with a well-known name (e.g., "sls_fcc")
// Returns 0 on success and fills srv; negative on error
int ipc_server_start(ipc_server_t* srv, const char* name,
                     volatile int* mission_go,
                     volatile int* throttle,
                     volatile int* abort_req,
                     int recv_thread_priority);

// Stop server
void ipc_server_stop(ipc_server_t* srv);

// Client helper: send a message to a named server and receive a reply
int ipc_client_send(const char* name, const sim_msg_t* msg, sim_reply_t* reply);

// Timer pulse: start periodic pulses delivered to the server channel
// code: pulse code (e.g., PULSE_TICK), value: integer value passed with pulse
// Returns a timer_t to manage lifetime; 0 on error
int timer_pulse_start(int chid, int period_ms, int code, int value, timer_t* out_timer);

#ifdef __cplusplus
}
#endif

#endif // SLS_QNX_IPC_H
