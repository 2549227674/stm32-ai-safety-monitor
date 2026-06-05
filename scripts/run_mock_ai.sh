#!/usr/bin/env bash
# run_mock_ai.sh —— 本机或 OPi5 上本地启动 mock AI 服务，便于先打通端边链路
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${OPI5_AI_PORT:-8080}"
echo "[mock-ai] http://0.0.0.0:$PORT  (/health, /api/infer/vision)"
OPI5_AI_PORT="$PORT" python3 edge/opi5-ai/service/app.py
