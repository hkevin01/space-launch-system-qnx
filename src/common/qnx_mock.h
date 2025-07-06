#ifndef QNX_MOCK_H
#define QNX_MOCK_H

/**
 * @file qnx_mock.h
 * @brief Mock QNX functions for Linux development and testing
 *
 * This file provides mock implementations of QNX-specific functions
 * to allow development and testing on Linux systems without QNX.
 */

#ifdef MOCK_QNX_BUILD

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

// Mock QNX message passing
typedef int chid_t;
typedef int coid_t;
typedef int rcvid_t;

// Mock message codes
#define _IO_MAX 0x100

// Mock message info structure (declare before use)
struct _msg_info
{
    uint32_t nd;
    uint32_t srcnd;
    pid_t pid;
    int32_t chid;
    int32_t scoid;
    int32_t coid;
    int16_t priority;
    int16_t flags;
    uint32_t msglen;
    uint32_t srcmsglen;
    uint32_t dstmsglen;
    int16_t zero[3];
};

// Mock QNX functions
static inline chid_t ChannelCreate(unsigned flags)
{
    (void)flags;
    static int next_chid = 1;
    return next_chid++;
}

static inline int ChannelDestroy(chid_t chid)
{
    (void)chid;
    return 0;
}

static inline coid_t ConnectAttach(uint32_t nd, pid_t pid, chid_t chid,
                                   unsigned index, int flags)
{
    (void)nd;
    (void)pid;
    (void)chid;
    (void)index;
    (void)flags;
    static int next_coid = 1;
    return next_coid++;
}

static inline int ConnectDetach(coid_t coid)
{
    (void)coid;
    return 0;
}

static inline rcvid_t MsgReceive(chid_t chid, void *msg, size_t bytes,
                                 struct _msg_info *info)
{
    (void)chid;
    (void)msg;
    (void)bytes;
    (void)info;
    // Mock: no message available
    errno = ETIMEDOUT;
    return -1;
}

static inline int MsgReply(rcvid_t rcvid, int status, const void *msg, size_t bytes)
{
    (void)rcvid;
    (void)status;
    (void)msg;
    (void)bytes;
    return 0;
}

static inline int MsgSend(coid_t coid, const void *smsg, size_t sbytes,
                          void *rmsg, size_t rbytes)
{
    (void)coid;
    (void)smsg;
    (void)sbytes;
    (void)rmsg;
    (void)rbytes;
    return 0;
}

// Mock thread naming (only if not already available)
#ifdef __linux__
// Linux already has pthread_setname_np, don't redefine
#else
static inline int pthread_setname_np(pthread_t thread, const char *name)
{
    (void)thread;
    (void)name;
    return 0;
}
#endif

// Mock QNX scheduling constants
#ifndef SCHED_FIFO
#define SCHED_FIFO 1
#endif

// Mock QNX-specific types
#ifndef __QNXNTO__
typedef uint32_t _Uint32t;
typedef int32_t _Int32t;
typedef uint16_t _Uint16t;
typedef int16_t _Int16t;
#endif

#else
// Real QNX includes
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/dispatch.h>
#endif // MOCK_QNX_BUILD

#endif // QNX_MOCK_H
