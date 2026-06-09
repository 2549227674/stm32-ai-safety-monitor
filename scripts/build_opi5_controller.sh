#!/usr/bin/env bash
# build_opi5_controller.sh -- Native compile on Orange Pi 5 (RK3588S)
# Usage: scripts/build_opi5_controller.sh [target]
#   Targets: opi5_safetyd (default), pca9685_set_pwm, all
set -euo pipefail
cd "$(dirname "$0")/.."

TARGET="${1:-opi5_safetyd}"
OUT_DIR="build/opi5-controller"
SRC_DIR="edge/opi5-controller/src"
INC_DIR="edge/opi5-controller/include"

echo "[build] OPi5 native compile (RK3588S aarch64)"
echo "[build] CC = ${CC:-gcc}"
${CC:-gcc} --version | head -1

mkdir -p "$OUT_DIR"

case "$TARGET" in
  opi5_safetyd)
    echo "[build] Building opi5_safetyd..."
    ${CC:-gcc} -Wall -Wextra -Wno-format-truncation -O2 \
      -I"$INC_DIR" \
      "$SRC_DIR/bsp_oled_ssd1306.c" \
      "$SRC_DIR/opi5_safetyd.c" \
      -o "$OUT_DIR/opi5_safetyd"
    echo "[build] Output: $OUT_DIR/opi5_safetyd"
    ;;
  pca9685_set_pwm)
    echo "[build] Building pca9685_set_pwm..."
    ${CC:-gcc} -Wall -Wextra -O2 \
      "$SRC_DIR/pca9685_set_pwm.c" \
      -o "$OUT_DIR/pca9685_set_pwm"
    echo "[build] Output: $OUT_DIR/pca9685_set_pwm"
    ;;
  all)
    "$0" opi5_safetyd
    "$0" pca9685_set_pwm
    echo "[build] All targets built."
    ;;
  *)
    echo "Unknown target: $TARGET"
    echo "Available: opi5_safetyd, pca9685_set_pwm, all"
    exit 1
    ;;
esac
