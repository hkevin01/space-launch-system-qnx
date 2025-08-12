#ifndef SLS_SLOG_H
#define SLS_SLOG_H

#include <slog2.h>

int sls_slog_init(void);

extern slog2_buffer_t slog_buf;

#define SLOGI(cat, fmt, ...) slog2c(slog_buf, 0, SLOG2_INFO,  "[" cat "] " fmt, ##__VA_ARGS__)
#define SLOGW(cat, fmt, ...) slog2c(slog_buf, 0, SLOG2_WARNING, "[" cat "] " fmt, ##__VA_ARGS__)
#define SLOGE(cat, fmt, ...) slog2c(slog_buf, 0, SLOG2_ERROR, "[" cat "] " fmt, ##__VA_ARGS__)

#endif // SLS_SLOG_H
