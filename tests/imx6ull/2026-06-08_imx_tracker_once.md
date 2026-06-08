# 2026-06-08 imx_tracker once Mode Test

## Target
imx_tracker minimal once flow: capture -> POST OPi5 -> parse bbox -> move gimbal -> write visual state.

## Modified Files
- `edge/imx6ull-controller/src/imx_tracker.c` (new)
- `scripts/build_imx6ull.sh` (added imx_tracker target)

## What Was Implemented
1. `imx_tracker.c`: Standalone C program for visual tracking
   - Captures MJPEG frame via v4l2-ctl
   - POSTs to OPi5 `/api/track/frame`
   - Parses `tracking_hint.offset_px` and `best_fire` from response
   - Computes pan/tilt step (deadband-aware, single-step clamped)
   - Calls `pca9685_set_pose` to move gimbal (tilt first, then pan)
   - Writes `/run/imx_visual_state.json` for imx_safetyd to read
   - Never controls pump/relay/buzzer (safety domain of imx_safetyd)
2. Environment variables: TRACK_STEP_US, TRACK_DEADBAND_PX, TRACK_PAN_MIN/MAX, TRACK_TILT_MIN/MAX
3. Modes: `--mode once` (single frame) and `--mode loop` (continuous)

## Verification
- [x] Cross-compile: `scripts/build_imx6ull.sh imx_tracker` succeeds
- [x] No warnings (unused function removed)
- [ ] Board-side `--mode once` with OPi5 YOLO running
- [ ] Moves gimbal toward detected target
- [ ] Writes visual state JSON
- [ ] Does NOT trigger pump

## Status
Code implemented, cross-compiled. **Board-side test pending deployment.**

## Real Hardware/Model
- Real RKNN: Depends on OPi5 backend (mock or real)
- Gimbal: Uses pca9685_set_pose (verified in Task03/08)
