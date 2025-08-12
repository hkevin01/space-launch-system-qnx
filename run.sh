#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}" )" && pwd)"

if [[ $# -gt 0 && ( "$1" == "-h" || "$1" == "--help" ) ]]; then
    echo "Usage: $0"
    echo "Builds and runs the QNX demo (simulation + operator console)."
    echo "On QNX, ensure qcc and slog2 are available."
    exit 0
fi

exec "$ROOT/scripts/qnx_run.sh"
