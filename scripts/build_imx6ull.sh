#!/usr/bin/env bash
# build_imx6ull.sh —— 在 WSL 中用 i.MX6ULL SDK 交叉编译（对应 Task02）
# 用法: scripts/build_imx6ull.sh [源文件|目标名] [输出名]
#   默认编译 edge/imx6ull-controller/src/hello.c -> build/imx6ull/hello_imx6ull
#   便捷目标: scripts/build_imx6ull.sh gpio_test -> build/imx6ull/gpio_test
#   便捷目标: scripts/build_imx6ull.sh pca9685_pwm_test -> build/imx6ull/pca9685_pwm_test
#   便捷目标: scripts/build_imx6ull.sh pca9685_servo_test -> build/imx6ull/pca9685_servo_test
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/lib_inventory.sh

SDK_ENV="$(inv_get imx6ull sdk_env || true)" # 例: /opt/fsl-imx-.../environment-setup-cortexa7...
TARGET_OR_SRC="${1:-hello}"
case "$TARGET_OR_SRC" in
  hello)
    SRC="edge/imx6ull-controller/src/hello.c"
    OUT_NAME="${2:-hello_imx6ull}"
    ;;
  gpio_test)
    SRC="edge/imx6ull-controller/src/gpio_test.c"
    OUT_NAME="${2:-gpio_test}"
    ;;
  pca9685_pwm_test)
    SRC="edge/imx6ull-controller/src/pca9685_pwm_test.c"
    OUT_NAME="${2:-pca9685_pwm_test}"
    ;;
  pca9685_servo_test)
    SRC="edge/imx6ull-controller/src/pca9685_servo_test.c"
    OUT_NAME="${2:-pca9685_servo_test}"
    ;;
  pca9685_set_pose)
    SRC="edge/imx6ull-controller/src/pca9685_set_pose.c"
    OUT_NAME="${2:-pca9685_set_pose}"
    ;;
  *)
    SRC="$TARGET_OR_SRC"
    OUT_NAME="${2:-hello_imx6ull}"
    ;;
esac
OUT_DIR="build/imx6ull"

if [[ -n "$SDK_ENV" && "$SDK_ENV" != *TODO_FILL* && "$SDK_ENV" != *"<"*">"* ]]; then
  # shellcheck disable=SC1090
  source "$SDK_ENV"                          # 注入 $CC / $CXX / $SDKTARGETSYSROOT
else
  CC="$(inv_require imx6ull cc)"
  export CC
  export PATH="$(dirname "$CC"):$PATH"        # Buildroot wrapper 需要同目录 toolchain-wrapper
fi

echo "[build] CC = ${CC:?SDK 未提供 \$CC}"
"$CC" --version | head -1

mkdir -p "$OUT_DIR"
# shellcheck disable=SC2086
$CC "$SRC" -o "$OUT_DIR/$OUT_NAME"
echo "[build] 产物: $OUT_DIR/$OUT_NAME"
