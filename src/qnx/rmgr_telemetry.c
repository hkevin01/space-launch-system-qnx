#include "rmgr_telemetry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>

#define RBUF_SZ 8192

static char ring[RBUF_SZ];
static size_t r_head = 0; // write position
static size_t r_tail = 0; // read position
static pthread_mutex_t rmtx = PTHREAD_MUTEX_INITIALIZER;

static resmgr_connect_funcs_t cfuncs;
static resmgr_io_funcs_t ifuncs;
static iofunc_attr_t ioattr;

static int io_read(resmgr_context_t* ctp, io_read_t* msg, RESMGR_OCB_T* ocb) {
    int nonblock = (ocb->ioflag & O_NONBLOCK);

    _IO_SET_READ_NBYTES(ctp, 0);

    if (msg->i.xtype & _IO_XTYPE_MASK) return _RESMGR_ERR(EINVAL);

    if (msg->i.nbytes <= 0) return _RESMGR_ERR(EINVAL);

    pthread_mutex_lock(&rmtx);
    size_t available = (r_head >= r_tail) ? (r_head - r_tail) : (RBUF_SZ - (r_tail - r_head));
    if (available == 0) {
        pthread_mutex_unlock(&rmtx);
        if (nonblock) return _RESMGR_ERR(EAGAIN);
        // For simplicity, return 0 (EOF-like) if no data yet
        return _RESMGR_NOREPLY;
    }

    size_t to_copy = (available < msg->i.nbytes) ? available : msg->i.nbytes;
    // Avoid splitting wrap-around for simplicity
    size_t end = (r_tail + to_copy <= RBUF_SZ) ? (r_tail + to_copy) : RBUF_SZ;
    int n1 = end - r_tail;

    RESMGR_MSGREAD(ctp, ctp->msg, n1, sizeof(*msg));
    memcpy(((char*)ctp->msg) + sizeof(*msg), ring + r_tail, n1);

    r_tail = (r_tail + n1) % RBUF_SZ;

    _IO_SET_READ_NBYTES(ctp, n1);
    pthread_mutex_unlock(&rmtx);
    return _RESMGR_NPARTS(0);
}

static int io_open(resmgr_context_t* ctp, io_open_t* msg, RESMGR_HANDLE_T* handle, void* extra) {
    return iofunc_open_default(ctp, msg, &ioattr, extra);
}

static void* rmgr_thread(void* arg) {
    (void)arg;
    resmgr_context_t* ctp = dispatch_create();
    if (ctp == NULL) return NULL;

    iofunc_attr_init(&ioattr, S_IFCHR | 0444, NULL, NULL);

    memset(&cfuncs, 0, sizeof(cfuncs));
    cfuncs.open = io_open;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &cfuncs, _RESMGR_IO_NFUNCS, &ifuncs);
    ifuncs.read = io_read;

    resmgr_attr_t rattr;
    memset(&rattr, 0, sizeof(rattr));

    int id = resmgr_attach(ctp, &rattr, "/dev/sls_telemetry", _FTYPE_ANY, 0, &cfuncs, &ifuncs, &ioattr);
    if (id == -1) {
        dispatch_destroy(ctp);
        return NULL;
    }

    while (1) {
        resmgr_context_t* rc = dispatch_block(ctp);
        if (!rc) break;
        dispatch_handler(rc);
    }

    dispatch_destroy(ctp);
    return NULL;
}

int rmgr_telemetry_start(rmgr_telemetry_t* ctx, const char* devname) {
    (void)devname; // Using fixed /dev/sls_telemetry in this minimal impl
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->running = 1;
    return pthread_create(&ctx->thread, NULL, rmgr_thread, NULL);
}

void rmgr_telemetry_stop(rmgr_telemetry_t* ctx) {
    if (!ctx) return;
    if (ctx->thread) {
        pthread_cancel(ctx->thread);
        pthread_join(ctx->thread, NULL);
        ctx->thread = 0;
    }
    ctx->running = 0;
}

void rmgr_telemetry_append(const char* line) {
    if (!line) return;
    pthread_mutex_lock(&rmtx);
    size_t len = strnlen(line, 512);
    for (size_t i = 0; i < len; ++i) {
        ring[r_head] = line[i];
        r_head = (r_head + 1) % RBUF_SZ;
        if (r_head == r_tail) { // overwrite oldest
            r_tail = (r_tail + 1) % RBUF_SZ;
        }
    }
    pthread_mutex_unlock(&rmtx);
}
