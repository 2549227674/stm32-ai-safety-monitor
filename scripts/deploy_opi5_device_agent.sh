#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

OPI5_HOST="${OPI5_HOST:-10.96.98.38}"
OPI5_USER="${OPI5_USER:-orangepi}"
OPI5_PASS="${OPI5_PASS:-orangepi}"
OPI5_DEPLOY="${OPI5_DEPLOY:-/opt/edge-ai-safety-monitor/opi5-device-agent}"

AGENT_SRC="$REPO_ROOT/edge/opi5-device-agent"

echo "[deploy] deploying device-agent to ${OPI5_USER}@${OPI5_HOST}:${OPI5_DEPLOY}"

sshpass -p "$OPI5_PASS" ssh -o StrictHostKeyChecking=no "${OPI5_USER}@${OPI5_HOST}" \
    "echo '${OPI5_PASS}' | sudo -S mkdir -p ${OPI5_DEPLOY} 2>/dev/null && echo '${OPI5_PASS}' | sudo -S chown ${OPI5_USER}:${OPI5_USER} ${OPI5_DEPLOY} 2>/dev/null"

sshpass -p "$OPI5_PASS" scp -o StrictHostKeyChecking=no \
    "$AGENT_SRC/app.py" \
    "$AGENT_SRC/video.py" \
    "$AGENT_SRC/metrics.py" \
    "$AGENT_SRC/mock_sensors.py" \
    "$AGENT_SRC/batcher.py" \
    "$AGENT_SRC/ai_observer.py" \
    "$AGENT_SRC/poster.py" \
    "${OPI5_USER}@${OPI5_HOST}:${OPI5_DEPLOY}/"

echo "[deploy] files deployed. Configure env and start service manually:"
echo "  ssh ${OPI5_USER}@${OPI5_HOST}"
echo "  sudo cp ${OPI5_DEPLOY}/config/opi5-device-agent.env /etc/edge-ai/"
echo "  sudo systemctl enable opi5-device-agent"
echo "  sudo systemctl start opi5-device-agent"
