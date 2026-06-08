# 2026-06-08 Spray Double-Confirmation Gate Test

## Target
imx_safetyd pump double-confirmation: local sensor + visual confirmation gate.

## Modified Files
- `edge/imx6ull-controller/src/imx_safetyd.c` (modified: spray confirmation gate)

## What Was Implemented
1. New config fields (env/CLI):
   - `SPRAY_CONFIRM_ENABLE` (0|1, default 0): master switch for double-confirmation
   - `SPRAY_REQUIRE_VISION` (0|1, default 1): require vision to activate pump
   - `SPRAY_MIN_AI_SCORE` (float, default 0.50): minimum fire score threshold
   - `VISUAL_STATE_PATH` (path, default /run/imx_visual_state.json): tracker writes here
   - `VISUAL_CONFIRM_WINDOW_MS` (ms, default 1500): freshness window
   - `VISION_OFFLINE_AUTOSPRAY` (0|1, default 1): auto-spray when vision offline
   - `SPRAY_MAX_MS` (ms, default 3000): max spray duration
   - `SPRAY_COOLDOWN_MS` (ms, default 2000): spray cooldown

2. Spray truth table:
   | local flame/mq2 | vision fire confirmed | pump |
   |:---:|:---:|:---:|
   | 1 | 1 | ON (SUPPRESSING) |
   | 1 | 0 / offline | OFF (AIMING) |
   | 0 | 1 | OFF (visual only, no spray) |
   | 0 | 0 | OFF (IDLE) |

3. `alarm_phase` added to event JSON (IDLE/AIMING/SUPPRESSING)
4. `spray_confirm` added to event JSON with full decision detail
5. `all_off()` still works (pump off on exit)
6. When `SPRAY_CONFIRM_ENABLE=0`, old behavior preserved (pump on with ALARM)

## Verification
- [x] Cross-compile: `scripts/build_imx6ull.sh imx_safetyd` succeeds
- [x] Forward declarations added, no warnings
- [ ] Scenario 1: local hazard + no vision -> ALARM, buzzer/RGB/relay ON, pump OFF, alarm_phase=AIMING
- [ ] Scenario 2: local hazard + vision confirmed -> ALARM, pump ON, alarm_phase=SUPPRESSING
- [ ] Scenario 3: vision only (no local) -> no ALARM, no pump
- [ ] SPRAY_CONFIRM_ENABLE=0 -> old behavior (pump on with ALARM)

## Status
Code implemented, cross-compiled. **Board-side test pending deployment.**

## Safety Notes
- `control_allowed=false` unchanged
- AI/OPi5 never controls pump directly
- `all_off()` guarantees pump=0 on exit
- Local sensor is always the necessary condition for spray
