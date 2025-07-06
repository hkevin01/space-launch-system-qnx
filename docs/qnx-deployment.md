# QNX Deployment and Operations Guide

## Overview

This document provides detailed instructions for deploying the Space Launch System simulation on QNX Neutrino RTOS, including system configuration, performance tuning, and operational procedures specific to QNX environments.

## QNX System Requirements

### Hardware Requirements

| Component | Minimum | Recommended | Enterprise |
|-----------|---------|-------------|------------|
| CPU | x86_64 dual-core | x86_64 quad-core | Multi-socket |
| Memory | 4 GB RAM | 8 GB RAM | 16+ GB RAM |
| Storage | 2 GB free | 10 GB free | 50+ GB SSD |
| Network | 100 Mbps | 1 Gbps | Redundant NICs |

### QNX Software Requirements

- **QNX Software Development Platform 7.1** or later
- **QNX Neutrino 7.1** RTOS
- **QNX Momentics IDE** (optional for development)
- **QNX BSP** (Board Support Package) for target hardware

## QNX System Configuration

### 1. Boot Configuration

Configure the QNX boot script (`build file`) for the space launch system:

```bash
# QNX build file for Space Launch System
[virtual=x86_64,bios +compress] .bootstrap = {
    startup-x86_64
    PATH=/proc/boot:/bin:/usr/bin:/opt/bin LD_LIBRARY_PATH=/proc/boot:/lib:/usr/lib:/lib/dll procnto-smp-instr
}

[+script] .script = {
    # Set system clock
    rtc hw
    
    # Start essential drivers
    devb-eide blk auto=partition dos exe=all
    devnp-e1000.so -d e1000 vid=0x8086,did=0x100e
    
    # Configure shared memory
    shm create 1M /tmp/sls_telemetry_shm
    shm create 512K /tmp/sls_command_shm
    shm create 256K /tmp/sls_status_shm
    
    # Set real-time priorities
    on -p 63 /opt/sls/bin/sls_main
    
    # Start subsystems with appropriate priorities
    on -p 50 /opt/sls/bin/flight_control
    on -p 45 /opt/sls/bin/engine_control
    on -p 40 /opt/sls/bin/telemetry_system
    
    # Mount filesystems
    mount -T io-pkt /lib/dll/devnp-e1000.so
    
    [+optional] shell
}

# Include binaries
/opt/sls/bin/sls_main=/home/kevin/Projects/space-launch-system-qnx/bin/sls_main
/opt/sls/bin/flight_control=/home/kevin/Projects/space-launch-system-qnx/bin/flight_control
/opt/sls/bin/engine_control=/home/kevin/Projects/space-launch-system-qnx/bin/engine_control
/opt/sls/bin/telemetry_system=/home/kevin/Projects/space-launch-system-qnx/bin/telemetry_system

# Configuration files
/opt/sls/config/system.conf=/home/kevin/Projects/space-launch-system-qnx/config/system.conf
```

### 2. Process Priority Configuration

Set appropriate priorities for real-time subsystems:

```c
// Priority definitions for QNX scheduling
#define PRIORITY_FLIGHT_CONTROL     50  // Highest priority for safety
#define PRIORITY_ENGINE_CONTROL     45  // High priority for propulsion
#define PRIORITY_SAFETY_MONITOR     60  // Critical safety systems
#define PRIORITY_TELEMETRY          40  // Data collection
#define PRIORITY_COMMUNICATION      35  // Ground communication
#define PRIORITY_USER_INTERFACE     20  // Lowest priority for GUI

// Set process priority at startup
struct sched_param param;
param.sched_priority = PRIORITY_FLIGHT_CONTROL;
if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("Failed to set real-time priority");
}
```

### 3. Memory Configuration

Configure shared memory regions for inter-process communication:

```bash
# Create shared memory regions during system startup
shmctl create /tmp/sls_telemetry 1048576    # 1MB for telemetry data
shmctl create /tmp/sls_commands 262144      # 256KB for commands
shmctl create /tmp/sls_status 131072        # 128KB for status updates
shmctl create /tmp/sls_logs 2097152         # 2MB for logging

# Set permissions
chmod 666 /tmp/sls_*
```

## Deployment Procedures

### 1. Build for QNX Target

```bash
# Set QNX environment
source /opt/qnx710/qnxsdp-env.sh

# Configure for target architecture
export QNX_TARGET=/opt/qnx710/target/qnx7
export QNX_HOST=/opt/qnx710/host/linux/x86_64

# Build with QNX toolchain
make clean
make QNX_BUILD=1 TARGET_ARCH=x86_64

# Create deployment package
tar czf sls-qnx-deploy.tgz bin/ config/ docs/
```

### 2. Transfer to QNX System

```bash
# Copy files to QNX target (via network or removable media)
scp sls-qnx-deploy.tgz qnx-target:/tmp/

# On QNX target, extract and install
ssh qnx-target
cd /tmp
tar xzf sls-qnx-deploy.tgz
cp -r bin/ /opt/sls/
cp -r config/ /opt/sls/
```

### 3. System Integration

```bash
# Install as QNX service
cp scripts/sls-service /etc/system/services/
chmod +x /etc/system/services/sls-service

# Configure automatic startup
echo "/etc/system/services/sls-service" >> /etc/rc.d/rc.local

# Test installation
/opt/sls/bin/sls_main --test-mode
```

## QNX-Specific Operations

### 1. Process Management

Monitor and manage QNX processes:

```bash
# List running processes with priorities
pidin -p pid,name,prio,state

# Check specific subsystem
pidin -p pid,name,prio,state | grep sls

# Monitor real-time performance
nice -n -20 dumper -p flight_control

# Kill process gracefully
slay flight_control

# Force kill if necessary
slay -9 flight_control
```

### 2. Inter-Process Communication

Monitor IPC channels and message queues:

```bash
# List active channels
ls /dev/mqueue/

# Monitor message traffic
echo 1 > /proc/sys/kernel/mqueue_msg_max

# Check shared memory usage
cat /proc/sysvipc/shm

# Monitor memory mapped regions
cat /proc/self/maps | grep sls
```

### 3. Resource Monitoring

Track system resources in real-time:

```bash
# CPU utilization by process
top -p $(pgrep sls)

# Memory usage
cat /proc/meminfo | grep -E "(MemTotal|MemFree|MemAvailable)"

# Real-time scheduling info
cat /proc/sched_debug | grep sls

# Interrupt latency
trace_sched_switch &
TRACE_PID=$!
sleep 10
kill $TRACE_PID
```

## Performance Tuning

### 1. Real-Time Optimization

Configure QNX for optimal real-time performance:

```bash
# Disable CPU power management
echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Set CPU affinity for critical processes
taskset -cp 0 $(pgrep flight_control)
taskset -cp 1 $(pgrep engine_control)

# Configure interrupt handling
echo 2 > /proc/irq/0/smp_affinity  # Timer interrupts to CPU 1
echo 4 > /proc/irq/8/smp_affinity  # Network to CPU 2

# Optimize memory allocation
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo 1 > /proc/sys/vm/compact_memory
```

### 2. Network Configuration

Optimize network for telemetry and communication:

```bash
# Configure network interface
ifconfig en0 192.168.1.100 netmask 255.255.255.0 up

# Set network buffer sizes
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.ipv4.tcp_rmem="4096 16384 16777216"
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# Enable TCP timestamping for precision
echo 1 > /proc/sys/net/ipv4/tcp_timestamps
```

### 3. I/O Optimization

Configure storage for logging and data recording:

```bash
# Mount with optimal flags for logging
mount -o noatime,nodiratime /dev/hd0t78 /var/log/sls

# Configure write barriers
echo 0 > /sys/block/hd0/queue/read_ahead_kb
echo mq-deadline > /sys/block/hd0/queue/scheduler

# Set I/O priorities
ionice -c 1 -n 0 $(pgrep telemetry)  # Real-time class for telemetry
ionice -c 2 -n 7 $(pgrep logging)    # Best-effort class for logging
```

## Troubleshooting

### 1. Common QNX Issues

**Process Won't Start with Real-Time Priority**
```bash
# Check available priorities
cat /proc/sys/kernel/sched_rt_period_us
cat /proc/sys/kernel/sched_rt_runtime_us

# Increase RT budget if needed
echo 1000000 > /proc/sys/kernel/sched_rt_runtime_us
```

**IPC Channel Creation Fails**
```bash
# Check channel limits
cat /proc/sys/fs/mqueue/msg_max
cat /proc/sys/fs/mqueue/msgsize_max

# Increase limits
echo 1024 > /proc/sys/fs/mqueue/msg_max
echo 65536 > /proc/sys/fs/mqueue/msgsize_max
```

**Memory Allocation Failures**
```bash
# Check memory usage
cat /proc/meminfo
cat /proc/buddyinfo

# Clear caches if needed
echo 3 > /proc/sys/vm/drop_caches
```

### 2. Performance Issues

**High Jitter in Real-Time Tasks**
```bash
# Check for interrupt storms
cat /proc/interrupts

# Isolate CPUs for critical tasks
echo 2-3 > /sys/devices/system/cpu/isolated

# Disable services on isolated CPUs
systemctl set-property --runtime user@.service CPUAffinity=0-1
```

**IPC Latency Too High**
```bash
# Monitor IPC performance
perf record -g ./sls_main
perf report

# Optimize message sizes
echo "Check message size in IPC calls - keep under 4KB for best performance"

# Use pulse instead of messages for simple notifications
# Replace MsgSend with pulse for time-critical alerts
```

## Security Considerations

### 1. Access Control

Configure proper permissions for space launch system:

```bash
# Create dedicated user for SLS
useradd -r -s /bin/false sls-system

# Set file permissions
chown -R sls-system:sls-system /opt/sls/
chmod 755 /opt/sls/bin/*
chmod 644 /opt/sls/config/*

# Configure capabilities instead of root
setcap cap_sys_nice+ep /opt/sls/bin/sls_main
setcap cap_ipc_lock+ep /opt/sls/bin/flight_control
```

### 2. Network Security

Secure communication channels:

```bash
# Configure firewall for telemetry
iptables -A INPUT -p tcp --dport 8080 -j ACCEPT  # GUI interface
iptables -A INPUT -p udp --dport 5555 -j ACCEPT  # Telemetry
iptables -A INPUT -j DROP  # Drop everything else

# Enable encryption for sensitive data
openssl req -x509 -newkey rsa:4096 -keyout /opt/sls/ssl/key.pem \
    -out /opt/sls/ssl/cert.pem -days 365 -nodes
```

## Maintenance Procedures

### 1. Log Management

```bash
# Rotate logs to prevent disk filling
logrotate -f /etc/logrotate.d/sls

# Archive old logs
tar czf /backup/sls-logs-$(date +%Y%m%d).tgz /var/log/sls/
find /var/log/sls/ -name "*.log" -mtime +7 -delete
```

### 2. System Updates

```bash
# Backup current system
tar czf /backup/sls-system-$(date +%Y%m%d).tgz /opt/sls/

# Test new version in development environment first
make clean && make test

# Deploy with rollback capability
cp -r /opt/sls /opt/sls.backup
cp -r bin/ /opt/sls/bin/
systemctl restart sls-service

# Monitor for 5 minutes, rollback if issues
if ! systemctl is-active sls-service; then
    cp -r /opt/sls.backup/* /opt/sls/
    systemctl start sls-service
fi
```

### 3. Health Monitoring

Set up continuous monitoring:

```bash
# Create monitoring script
cat > /opt/sls/scripts/health_check.sh << 'EOF'
#!/bin/bash
# Check if all subsystems are running
for subsystem in flight_control engine_control telemetry; do
    if ! pgrep "$subsystem" > /dev/null; then
        echo "ERROR: $subsystem not running"
        logger "SLS: $subsystem failed, attempting restart"
        systemctl restart sls-service
        break
    fi
done

# Check memory usage
MEM_USAGE=$(free | awk '/^Mem:/ {printf "%.0f", $3/$2 * 100}')
if [ "$MEM_USAGE" -gt 90 ]; then
    echo "WARNING: High memory usage: ${MEM_USAGE}%"
    logger "SLS: High memory usage: ${MEM_USAGE}%"
fi

# Check CPU usage
CPU_USAGE=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | awk -F'%' '{print $1}')
if (( $(echo "$CPU_USAGE > 80" | bc -l) )); then
    echo "WARNING: High CPU usage: ${CPU_USAGE}%"
    logger "SLS: High CPU usage: ${CPU_USAGE}%"
fi
EOF

chmod +x /opt/sls/scripts/health_check.sh

# Add to crontab
echo "*/5 * * * * /opt/sls/scripts/health_check.sh" | crontab -
```

## Appendix

### A. QNX Command Reference

Common QNX commands for system administration:

```bash
# Process management
pidin               # Process information
slay                # Kill process
on                  # Run program with attributes
nice                # Run with different priority

# IPC
ls /dev/mqueue/     # List message queues
ipcs                # Show IPC status
ipcrm               # Remove IPC objects

# System information
sin                 # System information
sin ver             # Version information
sin mem             # Memory information
sin cpu             # CPU information

# Resource managers
mount               # Mount filesystems
umount              # Unmount filesystems
fdisk               # Disk partitioning
```

### B. Configuration Templates

Sample configuration files are available in:
- `/opt/sls/config/templates/`
- Examples for different deployment scenarios
- Hardware-specific configurations

### C. Support Resources

- **QNX Documentation**: [www.qnx.com/developers/docs/](http://www.qnx.com/developers/docs/)
- **QNX Community**: [community.qnx.com](http://community.qnx.com)
- **Technical Support**: Contact QNX support for critical issues
- **Project Repository**: Internal documentation and issue tracking
