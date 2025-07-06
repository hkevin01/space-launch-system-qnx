/**
 * @file sls_ipc.c
 * @brief Implementation of QNX IPC system for Space Launch System
 */

#include "sls_ipc.h"
#include "sls_logging.h"
#include "sls_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/dispatch.h>

// Internal structures
typedef struct {
    int chid;
    char name[MAX_NAME_LENGTH];
    bool active;
} ipc_channel_t;

typedef struct {
    message_type_t type;
    int (*handler)(const ipc_message_t* msg);
} message_handler_t;

// Global IPC state
static bool g_ipc_initialized = false;
static ipc_channel_t g_channels[MAX_SUBSYSTEMS];
static int g_num_channels = 0;
static message_handler_t g_message_handlers[16];
static int g_num_handlers = 0;

// Internal function declarations
static int find_channel_by_name(const char* name);
static int create_ipc_message(message_type_t type, subsystem_type_t source,
                             subsystem_type_t dest, const void* data, 
                             size_t data_size, ipc_message_t** msg);

/**
 * @brief Initialize IPC subsystem
 */
int sls_ipc_init(void) {
    if (g_ipc_initialized) {
        return 0;
    }
    
    // Initialize channel array
    memset(g_channels, 0, sizeof(g_channels));
    g_num_channels = 0;
    
    // Initialize message handlers
    memset(g_message_handlers, 0, sizeof(g_message_handlers));
    g_num_handlers = 0;
    
    sls_log(LOG_LEVEL_INFO, "IPC", "IPC subsystem initialized");
    g_ipc_initialized = true;
    return 0;
}

/**
 * @brief Cleanup IPC subsystem
 */
void sls_ipc_cleanup(void) {
    if (!g_ipc_initialized) {
        return;
    }
    
    // Close all channels
    for (int i = 0; i < g_num_channels; i++) {
        if (g_channels[i].active) {
            sls_ipc_close_channel(g_channels[i].chid);
        }
    }
    
    g_ipc_initialized = false;
    sls_log(LOG_LEVEL_INFO, "IPC", "IPC subsystem cleaned up");
}

/**
 * @brief Create a new IPC channel
 */
int sls_ipc_create_channel(const char* channel_name) {
    if (!g_ipc_initialized) {
        return -1;
    }
    
    if (g_num_channels >= MAX_SUBSYSTEMS) {
        sls_log(LOG_LEVEL_ERROR, "IPC", "Maximum number of channels reached");
        return -1;
    }
    
    // Check if channel already exists
    if (find_channel_by_name(channel_name) >= 0) {
        sls_log(LOG_LEVEL_WARNING, "IPC", "Channel %s already exists", channel_name);
        return find_channel_by_name(channel_name);
    }
    
    // Create QNX channel
    int chid = ChannelCreate(0);
    if (chid == -1) {
        sls_log(LOG_LEVEL_ERROR, "IPC", "Failed to create channel %s: %s", 
               channel_name, strerror(errno));
        return -1;
    }
    
    // Store channel information
    ipc_channel_t* channel = &g_channels[g_num_channels];
    channel->chid = chid;
    sls_safe_strncpy(channel->name, channel_name, sizeof(channel->name));
    channel->active = true;
    
    g_num_channels++;
    
    sls_log(LOG_LEVEL_INFO, "IPC", "Created channel: %s (chid=%d)", channel_name, chid);
    return chid;
}

/**
 * @brief Connect to an existing IPC channel
 */
int sls_ipc_connect_channel(const char* channel_name) {
    if (!g_ipc_initialized) {
        return -1;
    }
    
    // For QNX, this would typically involve name_open() to connect to a named channel
    // For this simulation, we'll use a simplified approach
    int channel_idx = find_channel_by_name(channel_name);
    if (channel_idx >= 0) {
        return g_channels[channel_idx].chid;
    }
    
    // If channel doesn't exist locally, try to create it
    return sls_ipc_create_channel(channel_name);
}

/**
 * @brief Close an IPC channel
 */
void sls_ipc_close_channel(int chid) {
    // Find and deactivate channel
    for (int i = 0; i < g_num_channels; i++) {
        if (g_channels[i].chid == chid && g_channels[i].active) {
            ChannelDestroy(chid);
            g_channels[i].active = false;
            sls_log(LOG_LEVEL_INFO, "IPC", "Closed channel: %s", g_channels[i].name);
            break;
        }
    }
}

/**
 * @brief Send telemetry data
 */
int sls_ipc_send_telemetry(subsystem_type_t dest, const telemetry_point_t* data) {
    if (!data) {
        return -1;
    }
    
    ipc_message_t* msg;
    int result = create_ipc_message(MSG_TELEMETRY, SUBSYS_TELEMETRY, dest, 
                                   data, sizeof(telemetry_point_t), &msg);
    if (result != 0) {
        return result;
    }
    
    // For simulation, we'll just log the telemetry
    sls_log(LOG_LEVEL_DEBUG, "IPC", "Telemetry: %s = %.2f %s", 
           data->name, data->value, data->units);
    
    free(msg);
    return 0;
}

/**
 * @brief Send command
 */
int sls_ipc_send_command(subsystem_type_t dest, const command_t* cmd) {
    if (!cmd) {
        return -1;
    }
    
    ipc_message_t* msg;
    int result = create_ipc_message(MSG_COMMAND, SUBSYS_GROUND_SUPPORT, dest, 
                                   cmd, sizeof(command_t), &msg);
    if (result != 0) {
        return result;
    }
    
    sls_log(LOG_LEVEL_INFO, "IPC", "Command sent to %s: %s", 
           sls_subsystem_type_to_string(dest), cmd->command);
    
    free(msg);
    return 0;
}

/**
 * @brief Send status message
 */
int sls_ipc_send_status(subsystem_type_t dest, const status_message_t* status) {
    if (!status) {
        return -1;
    }
    
    ipc_message_t* msg;
    int result = create_ipc_message(MSG_STATUS, status->source, dest, 
                                   status, sizeof(status_message_t), &msg);
    if (result != 0) {
        return result;
    }
    
    sls_log(LOG_LEVEL_INFO, "IPC", "Status from %s: %s", 
           sls_subsystem_type_to_string(status->source), status->message);
    
    free(msg);
    return 0;
}

/**
 * @brief Send heartbeat
 */
int sls_ipc_send_heartbeat(subsystem_type_t source) {
    struct timespec timestamp;
    clock_gettime(CLOCK_REALTIME, &timestamp);
    
    ipc_message_t* msg;
    int result = create_ipc_message(MSG_HEARTBEAT, source, SUBSYS_FLIGHT_CONTROL, 
                                   &timestamp, sizeof(timestamp), &msg);
    if (result != 0) {
        return result;
    }
    
    sls_log(LOG_LEVEL_DEBUG, "IPC", "Heartbeat from %s", 
           sls_subsystem_type_to_string(source));
    
    free(msg);
    return 0;
}

/**
 * @brief Receive IPC message
 */
int sls_ipc_receive_message(int chid, ipc_message_t* msg, size_t max_size) {
    if (!msg || max_size < sizeof(ipc_message_t)) {
        return -1;
    }
    
    // For QNX, this would use MsgReceive()
    // For simulation, we'll return no message available
    return -1; // No message
}

/**
 * @brief Reply to a message
 */
int sls_ipc_reply_message(int rcvid, int reply_code, const void* reply_data, size_t reply_size) {
    // For QNX, this would use MsgReply()
    // For simulation, just log the reply
    sls_log(LOG_LEVEL_DEBUG, "IPC", "Reply sent: code=%d, size=%zu", reply_code, reply_size);
    return 0;
}

/**
 * @brief Broadcast telemetry to all interested subsystems
 */
int sls_ipc_broadcast_telemetry(const telemetry_point_t* data) {
    if (!data) {
        return -1;
    }
    
    // Broadcast to key subsystems that need telemetry
    subsystem_type_t targets[] = {
        SUBSYS_FLIGHT_CONTROL,
        SUBSYS_GROUND_SUPPORT,
        SUBSYS_TELEMETRY
    };
    
    int num_targets = sizeof(targets) / sizeof(targets[0]);
    int failures = 0;
    
    for (int i = 0; i < num_targets; i++) {
        if (sls_ipc_send_telemetry(targets[i], data) != 0) {
            failures++;
        }
    }
    
    return failures;
}

/**
 * @brief Broadcast status message
 */
int sls_ipc_broadcast_status(const status_message_t* status) {
    if (!status) {
        return -1;
    }
    
    // Broadcast to all subsystems
    int failures = 0;
    for (int i = 0; i < 8; i++) { // 8 subsystem types
        subsystem_type_t target = (subsystem_type_t)i;
        if (target != status->source) { // Don't send to self
            if (sls_ipc_send_status(target, status) != 0) {
                failures++;
            }
        }
    }
    
    return failures;
}

/**
 * @brief Broadcast emergency message
 */
int sls_ipc_broadcast_emergency(const char* emergency_msg) {
    if (!emergency_msg) {
        return -1;
    }
    
    status_message_t emergency_status = {
        .source = SUBSYS_FLIGHT_CONTROL,
        .state = STATE_EMERGENCY,
        .phase = PHASE_ABORT,
        .priority = PRIORITY_EMERGENCY,
        .error_code = 9999
    };
    
    sls_safe_strncpy(emergency_status.message, emergency_msg, 
                     sizeof(emergency_status.message));
    clock_gettime(CLOCK_REALTIME, &emergency_status.timestamp);
    
    sls_log(LOG_LEVEL_CRITICAL, "IPC", "EMERGENCY BROADCAST: %s", emergency_msg);
    
    return sls_ipc_broadcast_status(&emergency_status);
}

/**
 * @brief Process pending messages
 */
int sls_ipc_process_messages(void) {
    // In a real implementation, this would check all channels for pending messages
    // and dispatch them to registered handlers
    
    // For simulation, this is a no-op
    return 0;
}

/**
 * @brief Register a message handler
 */
int sls_ipc_register_message_handler(message_type_t type, 
                                    int (*handler)(const ipc_message_t* msg)) {
    if (!handler || g_num_handlers >= 16) {
        return -1;
    }
    
    g_message_handlers[g_num_handlers].type = type;
    g_message_handlers[g_num_handlers].handler = handler;
    g_num_handlers++;
    
    sls_log(LOG_LEVEL_DEBUG, "IPC", "Registered message handler for type %d", type);
    return 0;
}

/**
 * @brief Find channel by name
 */
static int find_channel_by_name(const char* name) {
    for (int i = 0; i < g_num_channels; i++) {
        if (g_channels[i].active && strcmp(g_channels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Create IPC message structure
 */
static int create_ipc_message(message_type_t type, subsystem_type_t source,
                             subsystem_type_t dest, const void* data, 
                             size_t data_size, ipc_message_t** msg) {
    if (!data || !msg) {
        return -1;
    }
    
    size_t total_size = sizeof(ipc_message_t) + data_size;
    *msg = (ipc_message_t*)malloc(total_size);
    if (!*msg) {
        sls_log(LOG_LEVEL_ERROR, "IPC", "Failed to allocate message memory");
        return -1;
    }
    
    (*msg)->type = type;
    (*msg)->source = source;
    (*msg)->destination = dest;
    (*msg)->sequence_number = 0; // Would be incremented in real implementation
    (*msg)->data_length = data_size;
    clock_gettime(CLOCK_REALTIME, &(*msg)->timestamp);
    
    memcpy((*msg)->data, data, data_size);
    
    return 0;
}

/**
 * @brief Get error string for IPC error code
 */
const char* sls_ipc_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "General error";
        case ENOENT: return "Channel not found";
        case ENOMEM: return "Out of memory";
        case ETIMEDOUT: return "Operation timed out";
        default: return "Unknown error";
    }
}
