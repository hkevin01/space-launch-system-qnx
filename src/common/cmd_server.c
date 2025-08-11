// cmd_server.c — simple TCP command server for GUI Chat
// POSIX implementation; for QNX, this should compile the same with qcc

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sls_logging.h"
#include "cmd_server.h"

#define CMD_PORT 5055
#define BUF_SZ 2048

static volatile int g_server_running = 0;
static int g_listen_fd = -1;

// Shared state (TODO: wire to real subsystems)
static int g_mission_go = 0;
static int g_engine_throttle = 0; // percent

int cmd_get_mission_go(void) { return g_mission_go; }
int cmd_get_engine_throttle(void) { return g_engine_throttle; }

static void handle_command(const char *line, char *out, size_t out_sz) {
  // Extremely naive JSON-ish parser for the commands we support
  // Expect examples:
  // {"cmd":"status"}\n
  if (strstr(line, "\"status\"")) {
    snprintf(out, out_sz,
             "{\"type\":\"status\",\"go\":%s,\"throttle\":%d}\n",
             g_mission_go ? "true" : "false", g_engine_throttle);
    return;
  }
  if (strstr(line, "\"go\"")) {
    g_mission_go = 1;
    snprintf(out, out_sz, "{\"type\":\"ack\",\"cmd\":\"go\"}\n");
    return;
  }
  if (strstr(line, "\"nogo\"")) {
    g_mission_go = 0;
    snprintf(out, out_sz, "{\"type\":\"ack\",\"cmd\":\"nogo\"}\n");
    return;
  }
  if (strstr(line, "\"abort\"")) {
    g_mission_go = 0;
    g_engine_throttle = 0;
    // TODO: trigger real abort sequence
    snprintf(out, out_sz, "{\"type\":\"ack\",\"cmd\":\"abort\"}\n");
    return;
  }
  if (strstr(line, "\"set_throttle\"")) {
    // find value
    const char *v = strstr(line, "\"value\"");
    if (v) {
      int val = atoi(v + 8);
      if (val < 0)
        val = 0;
      if (val > 100)
        val = 100;
      g_engine_throttle = val;
      snprintf(out, out_sz,
               "{\"type\":\"ack\",\"cmd\":\"set_throttle\",\"value\":%d}\n",
               val);
    } else {
      snprintf(out, out_sz,
               "{\"type\":\"error\",\"msg\":\"missing value\"}\n");
    }
    return;
  }

  snprintf(out, out_sz, "{\"type\":\"error\",\"msg\":\"unknown cmd\"}\n");
}

static void *client_thread(void *arg) {
  int sock = *(int *)arg;
  free(arg);
  char buf[BUF_SZ];
  char resp[BUF_SZ];

  while (g_server_running) {
    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
      break;
    buf[n] = 0;
    // process line-by-line
    char *saveptr = NULL;
    char *line = strtok_r(buf, "\n", &saveptr);
    while (line) {
      handle_command(line, resp, sizeof(resp));
      send(sock, resp, strlen(resp), 0);
      line = strtok_r(NULL, "\n", &saveptr);
    }
  }
  close(sock);
  return NULL;
}

static void *server_thread(void *unused) {
  (void)unused;

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    sls_log(LOG_LEVEL_ERROR, "CMD", "socket failed: %s", strerror(errno));
    return NULL;
  }
  g_listen_fd = s;

  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(CMD_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    sls_log(LOG_LEVEL_ERROR, "CMD", "bind failed: %s", strerror(errno));
    close(s);
    g_listen_fd = -1;
    return NULL;
  }

  if (listen(s, 8) < 0) {
    sls_log(LOG_LEVEL_ERROR, "CMD", "listen failed: %s", strerror(errno));
    close(s);
    g_listen_fd = -1;
    return NULL;
  }

  sls_log(LOG_LEVEL_INFO, "CMD", "listening on 127.0.0.1:%d", CMD_PORT);

  while (g_server_running) {
    int c = accept(s, NULL, NULL);
    if (c < 0) {
      if (!g_server_running)
        break;
      continue;
    }
    int *pc = (int *)malloc(sizeof(int));
    if (!pc) {
      close(c);
      continue;
    }
    *pc = c;
    pthread_t t;
    pthread_create(&t, NULL, client_thread, pc);
    pthread_detach(t);
  }

  if (g_listen_fd >= 0)
    close(g_listen_fd);
  g_listen_fd = -1;
  return NULL;
}

int cmd_server_start(void) {
  if (g_server_running)
    return 0;
  g_server_running = 1;
  pthread_t t;
  int rc = pthread_create(&t, NULL, server_thread, NULL);
  if (rc != 0) {
    g_server_running = 0;
    sls_log(LOG_LEVEL_ERROR, "CMD", "pthread_create failed: %d", rc);
    return -1;
  }
  pthread_detach(t);
  return 0;
}

void cmd_server_stop(void) {
  g_server_running = 0;
  if (g_listen_fd >= 0) {
    shutdown(g_listen_fd, SHUT_RDWR);
  }
}
