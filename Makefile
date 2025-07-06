# QNX Space Launch System Simulation Makefile

# Detect if QNX environment is available
QNX_AVAILABLE := $(shell command -v qcc 2> /dev/null)

# QNX SDK Configuration (if available)
QNX_HOST ?= /opt/qnx710/host/linux/x86_64
QNX_TARGET ?= /opt/qnx710/target/qnx7

# Build mode detection
ifdef QNX_AVAILABLE
    BUILD_MODE = qnx
    CC = qcc
    CXX = qcc
    AR = ntoaarch64-ar
    LD = ntoaarch64-ld
    ARCH = aarch64le
    PLATFORM_CFLAGS = -Vgcc_ntoaarch64le -DVARIANT_le -D_QNX_SOURCE
    PLATFORM_LDFLAGS = -Vgcc_ntoaarch64le -lang-c++
else
    BUILD_MODE = linux
    CC = gcc
    CXX = g++
    AR = ar
    LD = ld
    ARCH = x86_64
    PLATFORM_CFLAGS = -DMOCK_QNX_BUILD -D_GNU_SOURCE
    PLATFORM_LDFLAGS = 
endif

# Project directories
SRCDIR = src
BUILDDIR = build
BINDIR = bin
LIBDIR = lib
TESTDIR = tests

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -O2 -g $(PLATFORM_CFLAGS)
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g $(PLATFORM_CFLAGS)
LDFLAGS = -Wl,--gc-sections $(PLATFORM_LDFLAGS)

# Include directories
ifdef QNX_AVAILABLE
    INCLUDES = -Isrc/common -Isrc/subsystems -I$(QNX_TARGET)/usr/include
else
    INCLUDES = -Isrc/common -Isrc/subsystems
endif

# Libraries
ifdef QNX_AVAILABLE
    LIBS = -lm -lpthread -lsocket -lc
else
    LIBS = -lm -lpthread
endif

# Source files
COMMON_SRCS = $(wildcard $(SRCDIR)/common/*.c)
SUBSYSTEM_SRCS = $(wildcard $(SRCDIR)/subsystems/*.c)
UI_SRCS = $(wildcard $(SRCDIR)/ui/*.c)
MAIN_SRC = $(SRCDIR)/main.c

# Object files
COMMON_OBJS = $(COMMON_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
SUBSYSTEM_OBJS = $(SUBSYSTEM_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
UI_OBJS = $(UI_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
MAIN_OBJ = $(MAIN_SRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

ALL_OBJS = $(COMMON_OBJS) $(SUBSYSTEM_OBJS) $(UI_OBJS) $(MAIN_OBJ)

# Target executable
TARGET = $(BINDIR)/space_launch_sim

# Test files
TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_OBJS = $(TEST_SRCS:$(TESTDIR)/%.c=$(BUILDDIR)/tests/%.o)
TEST_TARGET = $(BINDIR)/run_tests

# Default target
.PHONY: all clean test install run help directories info

all: info directories $(TARGET)

# Show build information
info:
	@echo "Build Configuration:"
	@echo "  Build mode: $(BUILD_MODE)"
	@echo "  Compiler: $(CC)"
	@echo "  Architecture: $(ARCH)"
ifdef QNX_AVAILABLE
	@echo "  QNX Host: $(QNX_HOST)"
	@echo "  QNX Target: $(QNX_TARGET)"
else
	@echo "  Platform: Linux simulation mode"
	@echo "  Note: QNX features will be mocked for development"
endif
	@echo ""

# Create necessary directories
directories:
	@mkdir -p $(BUILDDIR)/common $(BUILDDIR)/subsystems $(BUILDDIR)/ui $(BUILDDIR)/tests
	@mkdir -p $(BINDIR) $(LIBDIR)

# Main executable
$(TARGET): $(ALL_OBJS)
	@echo "Linking $@..."
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Build complete: $@"

# Object file compilation rules
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test compilation
$(BUILDDIR)/tests/%.o: $(TESTDIR)/%.c
	@echo "Compiling test $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test target
test: directories $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) $(filter-out $(MAIN_OBJ), $(ALL_OBJS))
	@echo "Linking tests..."
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Run the simulation
run: $(TARGET)
	@echo "Starting Space Launch System Simulation..."
	@$(TARGET)

# Install to QNX target (if network mounted)
install: $(TARGET)
	@echo "Installing to QNX target..."
	@cp $(TARGET) /qnx_target/usr/bin/ 2>/dev/null || echo "QNX target not mounted"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILDDIR) $(BINDIR) $(LIBDIR)

# Development helpers
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

release: CFLAGS += -DRELEASE -O3 -DNDEBUG
release: clean $(TARGET)

# Static analysis
analyze:
	@echo "Running static analysis..."
	@clang-tidy $(SRCDIR)/**/*.c -- $(QNX_CFLAGS) $(CFLAGS) $(INCLUDES)

# Format code
format:
	@echo "Formatting code..."
	@clang-format -i $(SRCDIR)/**/*.c $(SRCDIR)/**/*.h

# Generate documentation
docs:
	@echo "Generating documentation..."
	@doxygen docs/Doxyfile

# Help target
help:
	@echo "QNX Space Launch System Simulation Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the main simulation (default)"
	@echo "  test      - Build and run tests"
	@echo "  run       - Run the simulation"
	@echo "  install   - Install to QNX target system"
	@echo "  clean     - Remove build artifacts"
	@echo "  debug     - Build debug version"
	@echo "  release   - Build optimized release version"
	@echo "  analyze   - Run static code analysis"
	@echo "  format    - Format source code"
	@echo "  docs      - Generate documentation"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  QNX_HOST=$(QNX_HOST)"
	@echo "  QNX_TARGET=$(QNX_TARGET)"
	@echo "  ARCH=$(ARCH)"

# Dependency generation
-include $(ALL_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

# Generate dependencies
$(BUILDDIR)/%.d: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(QNX_CFLAGS) $(CFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@
