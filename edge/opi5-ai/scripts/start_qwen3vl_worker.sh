#!/bin/bash
# start_qwen3vl_worker.sh — Start persistent Qwen3-VL worker on OPi5
# Usage: scripts/start_qwen3vl_worker.sh
set -euo pipefail
cd "$(dirname "$0")/.."

export QWEN3VL_ENCODER="${QWEN3VL_ENCODER:-/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn}"
export QWEN3VL_LLM="${QWEN3VL_LLM:-/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm}"
export QWEN3VL_CORE_NUM="${QWEN3VL_CORE_NUM:-3}"

echo "[worker] starting qwen3vl_worker..."
echo "[worker] encoder=$QWEN3VL_ENCODER"
echo "[worker] llm=$QWEN3VL_LLM"
echo "[worker] core_num=$QWEN3VL_CORE_NUM"

exec ./qwen3vl_worker
