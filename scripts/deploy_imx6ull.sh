#!/usr/bin/env bash
# deploy_imx6ull.sh —— 把交叉编译产物 scp 到 i.MX6ULL 并可选运行（对应 Task02/Task07）
# 用法: scripts/deploy_imx6ull.sh <本地产物路径> [--run]
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/lib_inventory.sh

ARTIFACT="${1:?用法: deploy_imx6ull.sh <本地产物路径> [--run]}"
RUN_FLAG="${2:-}"
IMX_HOST="$(inv_require imx6ull host)"
IMX_USER="$(inv_require imx6ull user)"
IMX_PORT="$(inv_get imx6ull ssh_port)"; IMX_PORT="${IMX_PORT:-22}"
IMX_DIR="$(inv_require imx6ull deploy_dir)"

ssh -p "$IMX_PORT" "$IMX_USER@$IMX_HOST" "mkdir -p '$IMX_DIR'"
scp -P "$IMX_PORT" "$ARTIFACT" "$IMX_USER@$IMX_HOST:$IMX_DIR/"
echo "[deploy] 已推送 $ARTIFACT -> $IMX_USER@$IMX_HOST:$IMX_DIR/"

if [[ "$RUN_FLAG" == "--run" ]]; then
  base="$(basename "$ARTIFACT")"
  ssh -p "$IMX_PORT" "$IMX_USER@$IMX_HOST" "chmod +x '$IMX_DIR/$base' && '$IMX_DIR/$base'"
fi
