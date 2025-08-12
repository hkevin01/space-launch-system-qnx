#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "qnx/ipc.h"

static void trim(char* s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) { s[--n] = 0; }
    while (*s && isspace((unsigned char)*s)) ++s; // note: not moving pointer in place for simplicity
}

int main(void) {
    printf("SLS Operator Console (QNX)\n");
    printf("Commands: status | go | nogo | abort | throttle <0-100> | quit\n");

    char line[128];
    while (1) {
        printf("> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        // trim in place minimal
        size_t len = strlen(line);
        if (len && (line[len-1] == '\n' || line[len-1] == '\r')) line[len-1] = 0;
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;

        sim_msg_t msg = { .type = CMD_STATUS, .value = 0 };
        if (strncmp(line, "throttle", 8) == 0) {
            int v = atoi(line + 8);
            msg.type = CMD_SET_THROTTLE; msg.value = v;
        } else if (strcmp(line, "status") == 0) {
            msg.type = CMD_STATUS;
        } else if (strcmp(line, "go") == 0) {
            msg.type = CMD_GO;
        } else if (strcmp(line, "nogo") == 0) {
            msg.type = CMD_NOGO;
        } else if (strcmp(line, "abort") == 0) {
            msg.type = CMD_ABORT;
        } else {
            printf("Unknown command\n");
            continue;
        }

        sim_reply_t rep;
        if (ipc_client_send("sls_fcc", &msg, &rep) == 0) {
            printf("ok=%d go=%d throttle=%d\n", rep.ok, rep.mission_go, rep.throttle);
        } else {
            printf("Failed to contact FCC\n");
        }
    }

    return 0;
}
