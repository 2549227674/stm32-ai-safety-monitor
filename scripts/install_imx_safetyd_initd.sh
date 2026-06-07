#!/usr/bin/env bash
# Install BusyBox / SysV init.d script for imx_safetyd onto i.MX6ULL
set -euo pipefail

cd "$(dirname "$0")/.."
source scripts/lib_inventory.sh

IMX_HOST="$(inv_require imx6ull host)"
IMX_USER="$(inv_require imx6ull user)"
IMX_PORT="$(inv_get imx6ull ssh_port)"
IMX_PORT="${IMX_PORT:-22}"
IMX_PASSWORD="$(inv_get imx6ull password || true)"

SSH_CMD=(ssh)
SCP_CMD=(scp)
if [[ -n "$IMX_PASSWORD" && "$IMX_PASSWORD" != *TODO_FILL* && "$IMX_PASSWORD" != *"<"*">"* ]]; then
    if ! command -v sshpass >/dev/null 2>&1; then
        echo "[install] inventory 配置了 imx6ull.password，但本机缺少 sshpass" >&2
        exit 1
    fi
    export SSHPASS="$IMX_PASSWORD"
    SSH_CMD=(sshpass -e ssh)
    SCP_CMD=(sshpass -e scp)
fi

"${SSH_CMD[@]}" -p "$IMX_PORT" -o StrictHostKeyChecking=accept-new "$IMX_USER@$IMX_HOST" "mkdir -p /etc/init.d /etc/edge-ai-safety-monitor"
"${SCP_CMD[@]}" -P "$IMX_PORT" -o StrictHostKeyChecking=accept-new edge/imx6ull-controller/init.d/S99imx-safetyd "$IMX_USER@$IMX_HOST:/tmp/S99imx-safetyd"
"${SSH_CMD[@]}" -p "$IMX_PORT" -o StrictHostKeyChecking=accept-new "$IMX_USER@$IMX_HOST" "cp /tmp/S99imx-safetyd /etc/init.d/S99imx-safetyd && chmod +x /etc/init.d/S99imx-safetyd"

echo "[install] 已安装 /etc/init.d/S99imx-safetyd"
echo "[install] 若板端尚无 /etc/edge-ai-safety-monitor/imx-safetyd.env，请手动创建；本脚本不会上传真实配置"
