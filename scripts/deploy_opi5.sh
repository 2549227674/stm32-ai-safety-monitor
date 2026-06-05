#!/usr/bin/env bash
# deploy_opi5.sh —— 把 OPi5 AI 服务同步到 Orange Pi 5（对应 Task05）
# 用法: scripts/deploy_opi5.sh [--restart]
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/lib_inventory.sh

OPI5_HOST="$(inv_require opi5 host)"
OPI5_USER="$(inv_require opi5 user)"
OPI5_PORT="$(inv_get opi5 ssh_port)"; OPI5_PORT="${OPI5_PORT:-22}"
OPI5_DIR="$(inv_require opi5 deploy_dir)"

ssh -p "$OPI5_PORT" "$OPI5_USER@$OPI5_HOST" "mkdir -p '$OPI5_DIR'"
# 只同步服务代码与脚本，绝不同步模型权重/图片（见 .gitignore）
scp -P "$OPI5_PORT" edge/opi5-ai/service/app.py "$OPI5_USER@$OPI5_HOST:$OPI5_DIR/"
echo "[deploy] 已推送 OPi5 AI 服务 -> $OPI5_USER@$OPI5_HOST:$OPI5_DIR/"

echo "[deploy] 在 OPi5 上启动(示例): OPI5_AI_PORT=<PORT> python3 $OPI5_DIR/app.py"
# >>> [CLAUDE_CODE_TODO | INVESTIGATE] 确认 OPi5 Python/flask/RKNN 依赖，必要时改为 systemd 管理
