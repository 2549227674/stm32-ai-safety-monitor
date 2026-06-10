#!/usr/bin/env bash
# deploy_opi5_all_in_one.sh — minimal OPi5 all-in-one deployment
# Run from project root on the OPi5 itself.
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV_FILE="/etc/edge-ai-safety-monitor.env"
DEPLOY_BASE="/opt/edge-ai-safety-monitor"
SERVICE_DIR="$PROJECT_ROOT/deploy/systemd"

echo "=== Edge AI Safety Monitor — OPi5 All-in-One Deploy ==="
echo "Project root: $PROJECT_ROOT"

# 1. Confirm project root
if [[ ! -f "$PROJECT_ROOT/server/backend/app.py" ]]; then
    echo "ERROR: must run from project root (server/backend/app.py not found)"
    exit 1
fi

# 2. Install / check Python dependencies
echo ""
echo "--- Python dependencies ---"
pip3 install --quiet -r "$PROJECT_ROOT/server/backend/requirements.txt" 2>/dev/null || \
    pip install --quiet -r "$PROJECT_ROOT/server/backend/requirements.txt"
echo "OK: backend requirements installed"

# 3. Build frontend
echo ""
echo "--- Build frontend ---"
if [[ -d "$PROJECT_ROOT/server/frontend/node_modules" ]]; then
    cd "$PROJECT_ROOT/server/frontend"
    npm run build
    echo "OK: frontend built -> server/backend/static/edge-console/"
else
    echo "SKIP: server/frontend/node_modules not found — run 'cd server/frontend && npm install && npm run build' first"
fi

# 4. Prepare deploy symlinks
echo ""
echo "--- Prepare deploy layout ---"
mkdir -p "$DEPLOY_BASE"/{server,edge,build/opi5-controller,run}

for d in server/backend server/frontend edge/opi5-device-agent edge/opi5-ai edge/opi5-controller; do
    src="$PROJECT_ROOT/$d"
    dst="$DEPLOY_BASE/$d"
    if [[ -d "$src" ]]; then
        if [[ ! -e "$dst" ]]; then
            ln -s "$src" "$dst"
            echo "  symlink: $dst -> $src"
        fi
    fi
done
# Symlink built safetyd binary
if [[ -f "$PROJECT_ROOT/build/opi5-controller/opi5_safetyd" ]]; then
    ln -sf "$PROJECT_ROOT/build/opi5-controller/opi5_safetyd" \
           "$DEPLOY_BASE/build/opi5-controller/opi5_safetyd"
    echo "  symlink: $DEPLOY_BASE/build/opi5-controller/opi5_safetyd"
fi

# 5. Generate env file if not exists
echo ""
echo "--- Environment file ---"
if [[ -f "$ENV_FILE" ]]; then
    echo "SKIP: $ENV_FILE already exists (not overwriting)"
else
    cp "$PROJECT_ROOT/config/opi5-all-in-one.env.example" "$ENV_FILE"
    echo "CREATED: $ENV_FILE (edit SMTP_PASSWORD before enabling email alerts)"
fi

# 6. SMTP notification config hint
echo ""
echo "--- SMTP notification config ---"
NOTIF_CFG="$PROJECT_ROOT/server/backend/notification_config.json"
if [[ -f "$NOTIF_CFG" ]]; then
    echo "SMTP config found: server/backend/notification_config.json"
    chmod 600 "$NOTIF_CFG" 2>/dev/null || true
else
    echo "SMTP config not found; configure in frontend notification settings or /etc/edge-ai-safety-monitor.env"
fi

# 7. Install systemd services
echo ""
echo "--- systemd services ---"
for svc in edge-ai-backend opi5-device-agent opi5-safetyd opi5-ai-qwen3vl; do
    src="$SERVICE_DIR/${svc}.service"
    dst="/etc/systemd/system/${svc}.service"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        echo "  installed: $dst"
    fi
done

systemctl daemon-reload
echo "OK: daemon-reload"

# 8. Enable services
echo ""
echo "--- Enable services ---"
for svc in edge-ai-backend opi5-device-agent opi5-safetyd opi5-ai-qwen3vl; do
    systemctl enable "$svc" 2>/dev/null && echo "  enabled: $svc" || echo "  (skip enable: $svc)"
done

# 9. Print summary
OPI5_IP=$(hostname -I 2>/dev/null | awk '{print $1}' || echo "<OPI5_IP>")
echo ""
echo "========================================="
echo "  Deploy complete!"
echo ""
echo "  Edit env:   sudo nano $ENV_FILE"
echo ""
echo "  Start all:  sudo systemctl start edge-ai-backend opi5-device-agent opi5-safetyd"
echo "  (AI service needs model files — start when ready)"
echo "  Start AI:   sudo systemctl start opi5-ai-qwen3vl"
echo ""
echo "  Stop:       sudo systemctl stop edge-ai-backend opi5-device-agent opi5-safetyd opi5-ai-qwen3vl"
echo ""
echo "  Logs:       journalctl -u edge-ai-backend -f"
echo "              journalctl -u opi5-device-agent -f"
echo "              journalctl -u opi5-safetyd -f"
echo ""
echo "  Browser:    http://${OPI5_IP}:5000"
echo "========================================="
