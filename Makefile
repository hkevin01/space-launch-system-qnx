## QNX-only Makefile — Space Launch System

CC       := qcc
CFLAGS   := -D_QNX_SOURCE -std=c11 -Wall -Wextra -Werror -O2
LDFLAGS  := -pthread -lslog2 -lresmgr -liofunc

SRC_DIR  := src
BLD_DIR  := build

# Binaries
SIM_BIN  := $(BLD_DIR)/sls_qnx
CON_BIN  := $(BLD_DIR)/sls_console

# Sources (QNX-only demo)
SIM_SRCS := \
    $(SRC_DIR)/qnx/main_qnx.c \
    $(SRC_DIR)/qnx/ipc.c \
    $(SRC_DIR)/qnx/rmgr_telemetry.c \
    $(SRC_DIR)/common/slog.c

CON_SRCS := \
    $(SRC_DIR)/ui/console.c \
    $(SRC_DIR)/qnx/ipc.c \
    $(SRC_DIR)/common/slog.c

SIM_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BLD_DIR)/%.o,$(SIM_SRCS))
CON_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BLD_DIR)/%.o,$(CON_SRCS))

INCLUDES := -I$(SRC_DIR) -I$(SRC_DIR)/qnx -I$(SRC_DIR)/common -I$(SRC_DIR)/ui

.PHONY: all clean run info

all: info $(SIM_BIN) $(CON_BIN)

info:
	@echo "QNX Build Configuration:"
	@echo "  CC       : $(CC)"
	@echo "  CFLAGS   : $(CFLAGS)"
	@echo "  LDFLAGS  : $(LDFLAGS)"
	@echo "  SIM_BIN  : $(SIM_BIN)"
	@echo "  CON_BIN  : $(CON_BIN)"
	@echo

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(SIM_BIN): $(SIM_OBJS)
	@mkdir -p $(dir $@)
	@echo "Linking $@"
	$(CC) $(SIM_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

$(CON_BIN): $(CON_OBJS)
	@mkdir -p $(dir $@)
	@echo "Linking $@"
	$(CC) $(CON_OBJS) $(LDFLAGS) -o $@
	@echo "Built: $@"

run: $(SIM_BIN) $(CON_BIN)
	./scripts/qnx_run.sh

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BLD_DIR)
