#!/usr/bin/env sh
set -e

SIM=build/sls_qnx
CON=build/sls_console
if [ ! -x "$SIM" ] || [ ! -x "$CON" ]; then
  echo "[qnx_run] Building first..."
  ./scripts/qnx_build.sh
fi

# Hints: set RR/FIFO scheduling if you have permissions; otherwise, run normally
# Example to start with priority: on -p70 $SIM

echo "Starting SLS QNX simulation..."
$SIM &
SIM_PID=$!
echo "Simulation PID: $SIM_PID"
echo "Telemetry: cat /dev/sls_telemetry"
echo "Starting operator console (connects to 'sls_fcc')..."
exec $CON
