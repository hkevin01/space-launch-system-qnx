#include "slog.h"
#include <string.h>

slog2_buffer_t slog_buf;

int sls_slog_init(void) {
    slog2_buffer_set_config_t config;
    memset(&config, 0, sizeof(config));
    const char* name = "SLS";
    slog2_register_params_t params = {0};
    params.buffer_set_name = name;
    params.num_buffers = 1;
    params.verbosity_level = SLOG2_INFO;

    if (slog2_register(&params, &slog_buf, 0) != 0) {
        return -1;
    }
    return 0;
}
