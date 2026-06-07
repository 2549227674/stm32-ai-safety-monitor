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
OPI5_PASSWORD="$(inv_get opi5 password || true)"

sh_quote() {
  local value="$1"
  printf "'%s'" "${value//\'/\'\\\'\'}"
}

SSH_CMD=(ssh)
SCP_CMD=(scp)
if [[ -n "$OPI5_PASSWORD" && "$OPI5_PASSWORD" != *TODO_FILL* && "$OPI5_PASSWORD" != *"<"*">"* ]]; then
  if ! command -v sshpass >/dev/null 2>&1; then
    echo "[deploy] inventory 配置了 opi5.password，但本机缺少 sshpass" >&2
    exit 1
  fi
  export SSHPASS="$OPI5_PASSWORD"
  SSH_CMD=(sshpass -e ssh)
  SCP_CMD=(sshpass -e scp)
fi

REMOTE_DIR="$(sh_quote "$OPI5_DIR")"
REMOTE_OWNER="$(sh_quote "$OPI5_USER:$OPI5_USER")"
if ! "${SSH_CMD[@]}" -p "$OPI5_PORT" -o StrictHostKeyChecking=accept-new "$OPI5_USER@$OPI5_HOST" "mkdir -p $REMOTE_DIR && test -w $REMOTE_DIR" 2>/dev/null; then
  echo "[deploy] 普通用户无法创建或写入 $OPI5_DIR，尝试使用 sudo 创建并授权给 $OPI5_USER"
  if [[ -n "$OPI5_PASSWORD" && "$OPI5_PASSWORD" != *TODO_FILL* && "$OPI5_PASSWORD" != *"<"*">"* ]]; then
    { sleep 0.2; printf '%s\n' "$OPI5_PASSWORD"; } | "${SSH_CMD[@]}" -tt -p "$OPI5_PORT" -o StrictHostKeyChecking=accept-new "$OPI5_USER@$OPI5_HOST" \
      "stty -echo; sudo -S -p '' -v; rc=\$?; stty echo; echo; [ \$rc -eq 0 ] && sudo mkdir -p $REMOTE_DIR && sudo chown $REMOTE_OWNER $REMOTE_DIR"
  else
    "${SSH_CMD[@]}" -tt -p "$OPI5_PORT" -o StrictHostKeyChecking=accept-new "$OPI5_USER@$OPI5_HOST" \
      "sudo -n mkdir -p $REMOTE_DIR && sudo -n chown $REMOTE_OWNER $REMOTE_DIR"
  fi
fi
# 只同步服务代码与脚本，绝不同步模型权重/图片（见 .gitignore）
"${SCP_CMD[@]}" -P "$OPI5_PORT" -o StrictHostKeyChecking=accept-new \
  edge/opi5-ai/service/app.py \
  edge/opi5-ai/service/qwen3vl_backend.py \
  edge/opi5-ai/service/risk_mapping.py \
  "$OPI5_USER@$OPI5_HOST:$OPI5_DIR/"
echo "[deploy] 已推送 OPi5 AI 服务 -> $OPI5_USER@$OPI5_HOST:$OPI5_DIR/"

echo "[deploy] 在 OPi5 上启动(示例): OPI5_AI_PORT=<PORT> python3 $OPI5_DIR/app.py"
# >>> [CLAUDE_CODE_TODO | INVESTIGATE] 确认 OPi5 Python/flask/RKNN 依赖，必要时改为 systemd 管理
