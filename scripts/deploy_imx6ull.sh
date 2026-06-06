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
IMX_PASSWORD="$(inv_get imx6ull password || true)"

SSH_CMD=(ssh)
SCP_CMD=(scp)
if [[ -n "$IMX_PASSWORD" && "$IMX_PASSWORD" != *TODO_FILL* && "$IMX_PASSWORD" != *"<"*">"* ]]; then
  if ! command -v sshpass >/dev/null 2>&1; then
    echo "[deploy] inventory 配置了 imx6ull.password，但本机缺少 sshpass" >&2
    exit 1
  fi
  export SSHPASS="$IMX_PASSWORD"
  SSH_CMD=(sshpass -e ssh)
  SCP_CMD=(sshpass -e scp)
fi

"${SSH_CMD[@]}" -p "$IMX_PORT" -o StrictHostKeyChecking=accept-new "$IMX_USER@$IMX_HOST" "mkdir -p '$IMX_DIR'"
"${SCP_CMD[@]}" -P "$IMX_PORT" -o StrictHostKeyChecking=accept-new "$ARTIFACT" "$IMX_USER@$IMX_HOST:$IMX_DIR/"
echo "[deploy] 已推送 $ARTIFACT -> $IMX_USER@$IMX_HOST:$IMX_DIR/"

if [[ "$RUN_FLAG" == "--run" ]]; then
  base="$(basename "$ARTIFACT")"
  "${SSH_CMD[@]}" -p "$IMX_PORT" -o StrictHostKeyChecking=accept-new "$IMX_USER@$IMX_HOST" "chmod +x '$IMX_DIR/$base' && '$IMX_DIR/$base'"
fi
