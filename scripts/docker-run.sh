#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}

# Allow connections to X11 on Linux host (optional)
if command -v xhost >/dev/null 2>&1; then
  xhost +local:docker >/dev/null 2>&1 || true
fi

# Start or reuse backend dev container
if ! docker ps --format '{{.Names}}' | grep -q '^sls-backend-dev$'; then
  docker compose -f "$ROOT/docker-compose.dev.yml" up -d backend
fi

# Exec into container and run the project
CMD=${1:-shell}
shift || true

case "$CMD" in
  shell)
    exec docker exec -it \
      -e DISPLAY="$DISPLAY" \
      -e QT_QPA_PLATFORM="$QT_QPA_PLATFORM" \
      sls-backend-dev bash
    ;;
  run)
    exec docker exec -it \
      -e DISPLAY="$DISPLAY" \
      -e QT_QPA_PLATFORM="$QT_QPA_PLATFORM" \
      sls-backend-dev bash -lc "./run.sh ${*:-}"
    ;;
  *)
    exec docker exec -it sls-backend-dev bash -lc "$CMD $*"
    ;;
esac
