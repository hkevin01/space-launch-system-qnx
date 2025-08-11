// cmd_server.h — simple TCP command server for GUI Chat
#ifndef CMD_SERVER_H
#define CMD_SERVER_H

int cmd_server_start(void);
void cmd_server_stop(void);

// Accessors for simple shared state (can be wired to real subsystems)
int cmd_get_mission_go(void);
int cmd_get_engine_throttle(void);

#endif // CMD_SERVER_H
