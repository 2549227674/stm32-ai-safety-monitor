# Task12 Hotfix: safety/json correctness

## Target
Fix 6 issues in bad6724 that affect safety, event JSON correctness, and board-side testability.

## Fixed

### Hotfix A: actuators.pump event consistency
- Added `actuator_buzzer/rgb_r/rgb_g/rgb_b/relay/pump` snapshot fields to `EventContext`
- `apply_actuators_by_state()` writes actual physical state to snapshot
- `build_event_json()` reads snapshot instead of re-deriving from state
- **Result**: When ALARM + local_hazard=1 + vision_hazard=0, event JSON shows `pump=0` (matches physical state)

### Hotfix B: matched_label JSON validity
- `matched_label` now properly quoted as `"fire"` or output as `null`
- Previously could output bare `fire` (invalid JSON)

### Hotfix C: visual freshness window
- `imx_tracker` now writes `monotonic_ms` to `/run/imx_visual_state.json`
- `imx_safetyd` reads `monotonic_ms` and compares with `now_ms()`
- Visual state older than `VISUAL_CONFIRM_WINDOW_MS` (default 1500ms) is discarded
- **Result**: Stale visual state cannot trigger SUPPRESSING

### Hotfix D: spray max duration and cooldown
- `SPRAY_MAX_MS` (default 3000ms): pump auto-stops after max duration
- `SPRAY_COOLDOWN_MS` (default 2000ms): pump won't restart during cooldown
- Both legacy and double-confirmation modes protected
- **Result**: Pump cannot run indefinitely; cooldown prevents rapid cycling

### Hotfix E: PCA9685 I2C lock
- fcntl-based file lock at `/run/edge-ai-safety-monitor/pca9685.lock`
- `imx_tracker` acquires lock before `pca9685_set_pose` calls
- `imx_safetyd` acquires lock before `pca9685_set_rgb/relay/pump` calls
- Lock directory auto-created if missing
- Lock failure logs WARNING and skips I2C write
- `all_off()` still attempts to turn off actuators

### Hotfix F: AI curl timeout configurable
- New config: `AI_CONNECT_TIMEOUT_SEC` (default 5), `AI_MAX_TIME_SEC` (default 20)
- CLI: `--ai-connect-timeout-sec`, `--ai-max-time-sec`
- `post_opi5_ai()` curl uses configurable timeouts
- **Result**: YOLO (fast) and Qwen3-VL (slow) can have different timeouts

### Hotfix G: docs/07 contract sync
- Added §7.10: `/api/track/frame` and `/api/track/stream` endpoint specs
- Added §7.11: `alarm_phase` and `spray_confirm` event field specs
- Added spray truth table

## Modified Files
- `edge/imx6ull-controller/src/imx_safetyd.c`
- `edge/imx6ull-controller/src/imx_tracker.c`
- `docs/07_端边HTTP_JSON_Contract.md`

## Verification
- [x] `py_compile` app.py + yolo_rknn_backend.py
- [x] `scripts/build_imx6ull.sh imx_tracker` — clean
- [x] `scripts/build_imx6ull.sh imx_safetyd` — clean
- [x] `grep "control_allowed.*true"` — only dashboard.js UI display
- [x] `grep "matched_label.*[a-zA-Z]"` — no bare values
- [ ] Generated event JSON passes `python3 -m json.tool` (board-side)
- [ ] Board-side dry-run: ALARM + no vision → pump=0, alarm_phase=AIMING
- [ ] Board-side dry-run: ALARM + vision → pump=1, alarm_phase=SUPPRESSING
- [ ] Loop mode spray max/cooldown

## Not Yet
- Real YOLO RKNN conversion
- Real water spray with tank
- Board-side loop mode spray timing verification
