#ifndef SLS_IPC_H
#define SLS_IPC_H

#include "sls_types.h"
#include <sys/neutrino.h>

/**
 * @file sls_ipc.h
 * @brief Inter-process communication system for QNX Space Launch System
 * 
 * This module provides QNX-specific IPC mechanisms including message passing,
 * channels, and shared memory for real-time communication between subsystems.
 */

// IPC Channel definitions
#define SLS_IPC_CHANNEL_MAIN        "/tmp/sls_main"
#define SLS_IPC_CHANNEL_TELEMETRY   "/tmp/sls_telemetry"
#define SLS_IPC_CHANNEL_COMMANDS    "/tmp/sls_commands"
#define SLS_IPC_CHANNEL_STATUS      "/tmp/sls_status"

// Message codes
#define MSG_CODE_TELEMETRY          (_IO_MAX + 1)
#define MSG_CODE_COMMAND            (_IO_MAX + 2)
#define MSG_CODE_STATUS             (_IO_MAX + 3)
#define MSG_CODE_HEARTBEAT          (_IO_MAX + 4)
#define MSG_CODE_EMERGENCY          (_IO_MAX + 5)

// IPC initialization and cleanup
int sls_ipc_init(void);
void sls_ipc_cleanup(void);

// Channel management
int sls_ipc_create_channel(const char* channel_name);
int sls_ipc_connect_channel(const char* channel_name);
void sls_ipc_close_channel(int chid);

// Message sending
int sls_ipc_send_telemetry(subsystem_type_t dest, const telemetry_point_t* data);
int sls_ipc_send_command(subsystem_type_t dest, const command_t* cmd);
int sls_ipc_send_status(subsystem_type_t dest, const status_message_t* status);
int sls_ipc_send_heartbeat(subsystem_type_t source);

// Message receiving
int sls_ipc_receive_message(int chid, ipc_message_t* msg, size_t max_size);
int sls_ipc_reply_message(int rcvid, int reply_code, const void* reply_data, size_t reply_size);

// Broadcast functions
int sls_ipc_broadcast_telemetry(const telemetry_point_t* data);
int sls_ipc_broadcast_status(const status_message_t* status);
int sls_ipc_broadcast_emergency(const char* emergency_msg);

// Message processing
int sls_ipc_process_messages(void);
int sls_ipc_register_message_handler(message_type_t type, 
                                    int (*handler)(const ipc_message_t* msg));

// Shared memory functions
int sls_ipc_create_shared_memory(const char* name, size_t size, void** ptr);
int sls_ipc_attach_shared_memory(const char* name, void** ptr);
void sls_ipc_detach_shared_memory(void* ptr);

// Priority messaging
int sls_ipc_send_priority_message(subsystem_type_t dest, const void* data, 
                                  size_t size, priority_level_t priority);

// Message queue utilities
int sls_ipc_get_queue_depth(int chid);
int sls_ipc_flush_queue(int chid);

// Error handling
const char* sls_ipc_error_string(int error_code);

#endif // SLS_IPC_H
