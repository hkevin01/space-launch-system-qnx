#!/usr/bin/env sh
set -e

echo "[qnx_build] Checking QNX environment..."
if [ -z "$QNX_HOST" ] || [ -z "$QNX_TARGET" ]; then
  echo "[qnx_build] QNX_HOST or QNX_TARGET not set. If building on a QNX target, this may be fine."
fi

make all

if [ -f build/sls_qnx ]; then
  echo "[qnx_build] Build success: build/sls_qnx"
else
  echo "[qnx_build] Build did not produce build/sls_qnx" >&2
  exit 1
fi
