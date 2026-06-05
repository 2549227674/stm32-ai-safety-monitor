#!/usr/bin/env bash
# build_imx6ull.sh —— 在 WSL 中用 i.MX6ULL SDK 交叉编译（对应 Task02）
# 用法: scripts/build_imx6ull.sh [源文件] [输出名]
#   默认编译 edge/imx6ull-controller/src/hello.c -> build/imx6ull/hello_imx6ull
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/lib_inventory.sh

SDK_ENV="$(inv_require imx6ull sdk_env)"     # 例: /opt/fsl-imx-.../environment-setup-cortexa7...
SRC="${1:-edge/imx6ull-controller/src/hello.c}"
OUT_NAME="${2:-hello_imx6ull}"
OUT_DIR="build/imx6ull"

# shellcheck disable=SC1090
source "$SDK_ENV"                            # 注入 $CC / $CXX / $SDKTARGETSYSROOT
echo "[build] CC = ${CC:?SDK 未提供 \$CC}"
"$CC" --version | head -1

mkdir -p "$OUT_DIR"
# shellcheck disable=SC2086
$CC "$SRC" -o "$OUT_DIR/$OUT_NAME"
echo "[build] 产物: $OUT_DIR/$OUT_NAME"

# >>> [CLAUDE_CODE_TODO | FILL] 确认 SDK environment-setup 路径与工具链三元组（写入 inventory.yaml: imx6ull.sdk_env）
