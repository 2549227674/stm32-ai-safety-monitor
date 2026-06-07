# Task11-D: Pump / Water Gun Dual-Load on PCA9685 CH6

## 1. Basic Info

| Item | Value |
|---|---|
| Branch | `migration/imx6ull-opi5-edge-ai` |
| Stage | Task11-D pump water tank |
| Date | 2026-06-07 |

## 2. Wiring

| Signal | Connection |
|---|---|
| PCA9685 CH6 PWM/S | MOS IO |
| PCA9685 GND | MOS GND |
| Independent 5V+ | MOS VIN+ |
| Independent 5V- / GND | MOS VIN- |
| Water pump red wire | MOS VOUT+ |
| Water pump black wire | MOS VOUT- |
| Water gun emitter red wire | MOS VOUT+ |
| Water gun emitter black wire | MOS VOUT- |
| Grounding | i.MX GND / PCA9685 GND / MOS GND / 5V PSU GND all common |

**Note**: This test's CH6 drives **dual-load: water pump + water gun emitter** (parallel on MOS output).

## 3. Default OFF Result

CH6 duty=0: both loads NOT running. **Pass**.

## 4. CH6 Duty Bare Test

| duty | Water pump | Water gun | MOS heat | Board |
|---|---|---|---|---|
| 0 | OFF | OFF | None | Online |
| 4095 | ON | ON | Normal | Online |

**pump_active_high = 1** (duty=4095 turns on both loads).

## 5. Dual-Load Short Pulse Result

- duty=4095 for 1s: both water pump and water gun emitter activated
- duty=0: both stopped
- No MOS overheat
- No power dip
- No smell
- Board remained online

**Pass** (dual-load verified).

## 6. Water Gun Demo

Short 2-second ON cycle:
- Water gun emitter responded and sprayed
- Water pump responded
- Both stopped on duty=0
- No water near circuit boards
- No leakage observed

## 7. Code Changes

Modified `edge/imx6ull-controller/src/imx_safetyd.c`:

- Added config: `PUMP_ENABLE` (default 0), `PCA9685_PUMP_CH` (default 6), `PCA9685_PUMP_ACTIVE_HIGH` (default 1)
- Added CLI args: `--pump-enable`, `--pca9685-pump-ch`, `--pca9685-pump-active-high`
- Added `pca9685_set_pump()` function (same pattern as relay)
- `apply_actuators_by_state()`: ALARM -> pump=1; NORMAL/VERIFY/FAULT -> pump=0
- `all_off()`: pump=0 on exit
- Event JSON: `actuators.pump` uses real state (was hardcoded 0)
- OLED: line3 shows `B%dR%dP%d` (buzzer/relay/pump states)
- Strong constraints: pump only from i.MX local state machine; soc_temp does NOT drive pump; AI/OPi5/Flask do NOT control pump

## 8. imx_safetyd Verification

### Scenario A: NORMAL

```bash
PUMP_ENABLE=1 RELAY_ENABLE=1 OLED_ENABLE=1 RGB_BACKEND=pca9685 \
POST_NORMAL=1 GPIO_MQ2_ACTIVE_HIGH=0 \
timeout 8 ./imx_safetyd --mode loop
```

| Check | Expected | Actual |
|---|---|---|
| state | NORMAL | NORMAL |
| pump | 0 | 0 (CH6 duty=0) |
| relay | 0 | 0 (CH5 duty=0) |
| buzzer | 0 | 0 |
| RGB | green | CH3 duty=4095 |
| Loads | not running | confirmed |

**Pass**

### Scenario B: ALARM (MQ-2 trigger)

```bash
PUMP_ENABLE=1 RELAY_ENABLE=1 OLED_ENABLE=1 RGB_BACKEND=pca9685 \
GPIO_MQ2_ACTIVE_HIGH=1 \
timeout 10 ./imx_safetyd --mode loop --interval-sec 3
```

| Check | Expected | Actual |
|---|---|---|
| state | ALARM | ALARM (mq2=1) |
| pump | 1 | 1 (CH6 duty=4095) |
| relay | 1 | 1 (CH5 duty=4095) |
| buzzer | 1 | 1 |
| RGB | red | CH2 duty=4095 |
| Dual-load | ON | confirmed (water pump + water gun both active) |

**Pass**

### Exit all_off

After timeout/SIGTERM:
- CH6 duty=0 (pump OFF, dual-load stopped)
- CH5 duty=0 (relay OFF)
- Buzzer stopped
- RGB off

**Pass**

## 9. Event JSON Sample (ALARM)

```json
{
  "state": "ALARM",
  "risk_score": 5,
  "sensors": {"door": 0, "pir": 0, "flame": 0, "mq2": 1, "soc_temp": 46.0},
  "actuators": {"relay": 1, "pump": 1, "buzzer": 1, "rgb_r": 1, "rgb_g": 0, "rgb_b": 0},
  "device_health": "NORMAL"
}
```

## 10. Conclusion

**Pass**: Task11-D pump water tank (dual-load) verified.

- PCA9685 CH6 controls MOS for dual-load: water pump + water gun emitter
- Default OFF: both loads not running at duty=0
- Active high: duty=4095 turns on both loads
- ALARM -> pump=1 (both loads activate)
- NORMAL/VERIFY/FAULT -> pump=0
- all_off ensures CH6 duty=0 on exit
- OLED displays pump state
- Event JSON includes real pump value
- No 220V, no water near circuits, no overheat

## 11. Boundaries

- No 220V connected
- No real high-power load
- Water pump/gun ran briefly only (not long duration)
- Water kept away from i.MX/PCA9685/power board
- soc_temp does NOT drive pump
- AI/OPi5/Flask do NOT control pump
- control_allowed=false
- DB schema unchanged
- No fan restored
- No humidity added
