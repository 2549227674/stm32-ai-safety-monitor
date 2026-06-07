# Task11-C: SoC Temperature Device Health

## 1. Basic Info

| Item | Value |
|---|---|
| Branch | `migration/imx6ull-opi5-edge-ai` |
| Stage | Task11-C SoC temperature health |
| Date | 2026-06-07 |

## 2. Temperature Source

| Item | Value |
|---|---|
| Path | `/sys/class/thermal/thermal_zone0/temp` |
| Fallback | `/sys/class/hwmon/hwmon0/temp1_input` (not needed) |
| Raw value | 46589 |
| Converted | 46.6°C |
| Type | SoC internal chip temperature (i.MX6ULL TEMPMON) |
| Not | Environment temperature; no humidity |

## 3. Code Changes

Modified `edge/imx6ull-controller/src/imx_safetyd.c`:

- Added config: `SOC_TEMP_ENABLE` (default 1), `SOC_TEMP_PATH` (default `/sys/class/thermal/thermal_zone0/temp`), `SOC_TEMP_WARN_C` (default 70)
- Added CLI args: `--soc-temp-enable`, `--soc-temp-path`, `--soc-temp-warn-c`
- Added `read_soc_temp()`: reads millidegrees from sysfs, converts to float °C
- Added `device_health` field to EventContext: `NORMAL` / `WARN` / `UNKNOWN`
- Event JSON: `sensors.soc_temp` (float or null), `device_health` (string)
- Status JSON: `soc_temp`, `device_health`
- OLED: line1 shows `H:NORMAL` / `H:WARN`, line2 shows `Temp:46.6C` or `Temp: N/A`
- Strong constraints: soc_temp does NOT change state/risk_score/relay/pump; independent of fire ALARM

## 4. Scenario A: Normal (warn threshold high)

```bash
SOC_TEMP_ENABLE=1 SOC_TEMP_WARN_C=90 POST_NORMAL=1 GPIO_MQ2_ACTIVE_HIGH=0 \
OLED_ENABLE=1 RGB_BACKEND=pca9685 RELAY_ENABLE=1 \
timeout 8 ./imx_safetyd --mode loop
```

| Check | Expected | Actual |
|---|---|---|
| soc_temp valid | yes | yes |
| soc_temp value | ~46°C | 46.0°C |
| device_health | NORMAL | NORMAL |
| state | NORMAL | NORMAL |
| risk_score | 0 | 0 |
| relay | 0 | 0 (CH5 duty=0) |
| OLED line2 | Temp:46.0C | confirmed |

**Pass**

## 5. Scenario B: Low threshold simulating WARN

```bash
SOC_TEMP_ENABLE=1 SOC_TEMP_WARN_C=30 POST_NORMAL=1 GPIO_MQ2_ACTIVE_HIGH=0 \
OLED_ENABLE=1 RGB_BACKEND=pca9685 RELAY_ENABLE=1 \
timeout 8 ./imx_safetyd --mode loop
```

| Check | Expected | Actual |
|---|---|---|
| soc_temp valid | yes | yes |
| soc_temp value | ~46°C | 46.6°C |
| device_health | WARN | WARN |
| state | NORMAL (not ALARM) | NORMAL |
| risk_score | 0 (not increased) | 0 |
| relay | 0 (not driven by soc_temp) | 0 (CH5 duty=0) |
| OLED line1 | H:WARN | confirmed |

**Pass**

## 6. Event JSON Sample (Scenario B)

```json
{
  "sensors": {"door": 0, "pir": 0, "flame": 0, "mq2": 0, "soc_temp": 46.6},
  "device_health": "WARN",
  "state": "NORMAL",
  "risk_score": 0
}
```

## 7. Status JSON Sample (Scenario B)

```json
{
  "soc_temp": 46.6,
  "device_health": "WARN",
  "last_state": "NORMAL",
  "last_risk_score": 0
}
```

## 8. Conclusion

**Pass**: Task11-C SoC temperature device health monitoring verified.

- `/sys/class/thermal/thermal_zone0/temp` readable, returns millidegrees (46589 = 46.6°C)
- `device_health=WARN` when `soc_temp >= SOC_TEMP_WARN_C`
- `device_health=NORMAL` when `soc_temp < SOC_TEMP_WARN_C`
- soc_temp does NOT affect state machine (no ALARM from temperature)
- soc_temp does NOT affect risk_score
- soc_temp does NOT drive relay/pump
- OLED displays temperature and health status
- Event JSON includes `sensors.soc_temp` and `device_health`
- Status JSON includes `soc_temp` and `device_health`

## 9. Boundaries

- No new hardware connected
- No fan (removed from P1)
- No pump (Task11-D pending)
- No 220V
- soc_temp is NOT environment temperature
- No humidity field added
- soc_temp does NOT trigger fire ALARM
- AI/OPi5/Flask do not control actuators
- control_allowed=false
- DB schema unchanged (raw_json passthrough)
