#!/bin/bash
# start_ai_service_qwen3vl.sh — Start Flask AI service in Qwen3-VL worker mode
# Usage: scripts/start_ai_service_qwen3vl.sh
# Requires: qwen3vl_worker already running (see start_qwen3vl_worker.sh)
set -euo pipefail
cd "$(dirname "$0")/.."

export AI_BACKEND="${AI_BACKEND:-qwen3vl}"
export QWEN3VL_MODE="${QWEN3VL_MODE:-worker}"
export QWEN3VL_ENCODER="${QWEN3VL_ENCODER:-/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn}"
export QWEN3VL_LLM="${QWEN3VL_LLM:-/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm}"
export QWEN3VL_CORE_NUM="${QWEN3VL_CORE_NUM:-3}"
export OPI5_AI_PORT="${OPI5_AI_PORT:-8080}"

echo "[ai-service] starting with AI_BACKEND=$AI_BACKEND QWEN3VL_MODE=$QWEN3VL_MODE"
echo "[ai-service] port=$OPI5_AI_PORT"

exec python3 app.py
