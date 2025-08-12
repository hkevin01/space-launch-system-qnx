#include "ipc.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/netmgr.h>
#include <sys/siginfo.h>

typedef struct {
    int rcvid;
    struct _msg_info info;
} recv_ctx_t;

static void* recv_loop(void* arg) {
    ipc_server_t* srv = (ipc_server_t*)arg;
    int chid = srv->chid;

    sim_msg_t msg;
    struct _pulse pulse;

    while (1) {
        int rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            if (errno == EINTR) continue;
            break;
        }
        if (rcvid == 0) {
            // Pulse received
            memcpy(&pulse, &msg, sizeof(pulse));
            if (pulse.code == PULSE_TICK) {
                // TICK pulse; nothing to reply
                // Application can poll shared state in main loop
            }
            continue;
        }

        // Message from client
        sim_reply_t reply = { .ok = 1, .mission_go = srv->mission_go ? *srv->mission_go : 0,
                              .throttle = srv->throttle ? *srv->throttle : 0 };

        switch (msg.type) {
            case CMD_STATUS:
                // No state change
                break;
            case CMD_GO:
                if (srv->mission_go) *srv->mission_go = 1;
                if (srv->abort_req) *srv->abort_req = 0;
                break;
            case CMD_NOGO:
                if (srv->mission_go) *srv->mission_go = 0;
                break;
            case CMD_ABORT:
                if (srv->abort_req) *srv->abort_req = 1;
                if (srv->mission_go) *srv->mission_go = 0;
                break;
            case CMD_SET_THROTTLE:
                if (srv->throttle) {
                    int v = msg.value;
                    if (v < 0) v = 0; if (v > 100) v = 100;
                    *srv->throttle = v;
                }
                break;
            default:
                reply.ok = 0;
                break;
        }

        // Update reply state after handling
        reply.mission_go = srv->mission_go ? *srv->mission_go : 0;
        reply.throttle   = srv->throttle ? *srv->throttle : 0;

        MsgReply(rcvid, EOK, &reply, sizeof(reply));
    }

    return NULL;
}

int ipc_server_start(ipc_server_t* srv, const char* name,
                     volatile int* mission_go,
                     volatile int* throttle,
                     volatile int* abort_req,
                     int recv_thread_priority) {
    if (!srv || !name) return -1;
    memset(srv, 0, sizeof(*srv));

    srv->attach = name_attach(NULL, name, 0);
    if (!srv->attach) {
        return -1;
    }
    srv->chid = srv->attach->chid;
    srv->mission_go = mission_go;
    srv->throttle = throttle;
    srv->abort_req = abort_req;
    srv->prio = recv_thread_priority;

    // Receiver thread
    pthread_attr_t attr; pthread_attr_init(&attr);
    struct sched_param sp = { .sched_priority = recv_thread_priority };
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    pthread_attr_setschedparam(&attr, &sp);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    int rc = pthread_create(&srv->thread, &attr, recv_loop, srv);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        name_detach(srv->attach, 0);
        memset(srv, 0, sizeof(*srv));
        return -1;
    }
    return 0;
}

void ipc_server_stop(ipc_server_t* srv) {
    if (!srv) return;
    if (srv->attach) {
        name_detach(srv->attach, 0);
        srv->attach = NULL;
    }
    if (srv->thread) {
        pthread_cancel(srv->thread);
        pthread_join(srv->thread, NULL);
        srv->thread = 0;
    }
}

int ipc_client_send(const char* name, const sim_msg_t* msg, sim_reply_t* reply) {
    if (!name || !msg) return -1;
    int coid = name_open(name, 0);
    if (coid == -1) return -1;

    sim_reply_t rep = {0};
    int rc = MsgSend(coid, msg, sizeof(*msg), &rep, sizeof(rep));
    name_close(coid);
    if (rc == -1) return -1;
    if (reply) *reply = rep;
    return 0;
}

int timer_pulse_start(int chid, int period_ms, int code, int value, timer_t* out_timer) {
    if (chid <= 0 || period_ms <= 0) return 0;

    struct sigevent sev;
    int coid = ConnectAttach(ND_LOCAL_NODE, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) return 0;

    SIGEV_PULSE_INIT(&sev, coid, SIGEV_PULSE_PRIO_INHERIT, code, value);

    timer_t timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        ConnectDetach(coid);
        return 0;
    }

    struct itimerspec its;
    its.it_value.tv_sec = period_ms / 1000;
    its.it_value.tv_nsec = (period_ms % 1000) * 1000000L;
    its.it_interval = its.it_value;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        timer_delete(timerid);
        ConnectDetach(coid);
        return 0;
    }

    if (out_timer) *out_timer = timerid;
    return 1;
}
