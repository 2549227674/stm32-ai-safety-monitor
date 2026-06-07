#!/usr/bin/env bash
# lib_inventory.sh —— 读取 config/inventory.yaml 的本地真实值
# 被 build/deploy 脚本 source。inventory.yaml 不入库（见 .gitignore）。
#
# 用法：
#   source scripts/lib_inventory.sh
#   IMX_IP="$(inv_get imx6ull host)"
#
# inventory.yaml 结构见 config/inventory.example.yaml（两级：section -> key）。

INVENTORY="${INVENTORY:-config/inventory.yaml}"

# inv_get <section> <key>
inv_get() {
  local section="$1" key="$2"
  if [[ ! -f "$INVENTORY" ]]; then
    echo "[inventory] 找不到 $INVENTORY，请先: cp config/inventory.example.yaml config/inventory.yaml 并填值" >&2
    return 1
  fi
  awk -v sec="$section" -v k="$key" '
    /^[A-Za-z0-9_]+:/ { cur=$1; sub(":","",cur); next }
    {
      line=$0
      sub(/^[[:space:]]+/, "", line)
      if (cur==sec && index(line, k ":")==1) {
        sub("^" k ":[[:space:]]*", "", line)
        gsub(/^["'"'"']|["'"'"']$/, "", line)   # 去掉首尾引号
        print line
        exit
      }
    }
  ' "$INVENTORY"
}

# inv_require <section> <key> —— 取值并校验未留占位符
inv_require() {
  local v
  v="$(inv_get "$1" "$2")" || return 1
  if [[ -z "$v" || "$v" == *TODO_FILL* || "$v" == *"<"*">"* ]]; then
    echo "[inventory] $1.$2 仍是占位符或为空，请在 $INVENTORY 中填写真实值" >&2
    return 1
  fi
  printf '%s' "$v"
}
