# ESP32-CAM WiFi HTTP Test Firmware

Stage 4 only verifies ESP32-CAM networking. It does not connect to STM32, does not use UART, does not take photos, does not use microSD, and does not control relays, fans, pumps, buzzers, RGB LEDs, or any other local actuator.

Use the ESP32-CAM + camera + USB flashing base kit only for USB power, serial download, and serial logs in this first version. Do not connect STM32, sensors, 12V power, relay boards, fan, or pump.

## What It Does

- Connects to WiFi.
- Sends one fixed `TEST` event to Flask every 5 seconds.
- Prints WiFi status, ESP32-CAM IP, server URL, HTTP response code, and error messages on Serial at 115200.
- Retries WiFi when disconnected.
- Times out HTTP operations so an unreachable server does not block the main loop forever.

## Files

```text
firmware/esp32cam/
├── platformio.ini
├── src/main.cpp
├── include/config.example.h
├── include/config.h
├── .gitignore
└── README.md
```

`include/config.h` is ignored by Git because it may contain real WiFi credentials. If it is ever tracked by mistake, run:

```bash
git rm --cached firmware/esp32cam/include/config.h
```

## Install PlatformIO

Recommended options:

- VS Code + PlatformIO IDE extension.
- PlatformIO Core:

```bash
python -m pip install platformio
```

## Configure WiFi and Flask Server

Create local config if needed:

```bash
cd firmware/esp32cam
cp include/config.example.h include/config.h
```

Edit `include/config.h`:

```text
WIFI_SSID
WIFI_PASSWORD
SERVER_HOST
SERVER_PORT
```

`SERVER_HOST` must be the computer/server LAN IPv4 address, for example `192.168.1.100`. Do not use `localhost`, because on ESP32-CAM it means the ESP32-CAM itself, not your PC.

Keep:

```text
API_ENDPOINT = "/api/events"
HEALTH_ENDPOINT = "/health"
DEVICE_ID = "labbox_001"
POST_INTERVAL_MS = 5000
```

## Start Flask First

From the repository root:

```bash
cd server/backend
python app.py
```

Confirm from the PC:

```bash
curl http://localhost:5000/health
```

For ESP32-CAM access, use:

```text
http://<your-pc-lan-ip>:5000/health
http://<your-pc-lan-ip>:5000/api/events
```

Make sure Flask listens on `0.0.0.0:5000` and that the firewall allows inbound TCP port `5000`.

## Build, Upload, Monitor

Plug the ESP32-CAM into the USB flashing base, connect the base to the PC with USB, then run:

```bash
cd firmware/esp32cam
pio run
pio run -t upload
pio device monitor -b 115200
```

If multiple serial ports exist, list them and specify the upload port:

```bash
pio device list
pio run -t upload --upload-port COM5
pio device monitor -b 115200 --port COM5
```

On Linux/macOS the port may look like `/dev/ttyUSB0` or `/dev/cu.usbserial-*`.

## Manual Flashing Notes

Most ESP32-CAM USB bases support automatic download mode. If upload fails:

- Check that the serial port is detected.
- Try another USB cable or USB port.
- Lower `upload_speed` in `platformio.ini` from `921600` to `115200`.
- Check whether the base supports automatic download.
- If there is no automatic download, connect `IO0` to `GND`, press reset, upload, then remove `IO0` from `GND` and reset again to run.
- If you see brownout or repeated resets, the ESP32-CAM power supply may be insufficient.

## Serial Output Example

```text
=== ESP32-CAM Safety Monitor HTTP Test ===
[mode] stage 4: WiFi + HTTP TEST event only
[mode] no camera, no UART, no local actuators
[server] http://192.168.1.100:5000/api/events
[health] http://192.168.1.100:5000/health
[wifi] disconnected, connecting...
[wifi] connected, ip=192.168.1.88, rssi=-52 dBm
[post] url=http://192.168.1.100:5000/api/events
[post] seq=1 payload={"type":"event",...}
[post] http_code=201 body={"event_id":1,"ok":true,...}
```

## Event JSON

The firmware sends this shape every 5 seconds:

```json
{
  "type": "event",
  "device_id": "labbox_001",
  "seq": 1,
  "state": "TEST",
  "risk_score": 0,
  "need_snap": false,
  "sensors": {"door": 0, "pir": 0, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 0}
}
```

## Acceptance Steps

1. Start Flask backend and open the Dashboard.
2. Edit `include/config.h` with WiFi and PC LAN IP.
3. Build with `pio run`.
4. Insert ESP32-CAM into the USB flashing base and connect USB.
5. Upload with `pio run -t upload`.
6. Open Serial Monitor at 115200.
7. Confirm WiFi connects and prints an ESP32-CAM IP.
8. Confirm Serial prints HTTP `201` or another 2xx code every 5 seconds.
9. Confirm the Dashboard shows repeated `state=TEST`, `risk_score=0` events.
10. Stop Flask and confirm ESP32-CAM prints HTTP errors but keeps retrying.

## Troubleshooting

### Serial Port Not Found

- Check Device Manager on Windows or `pio device list`.
- Replug the USB base.
- Try another USB cable.
- Install the USB serial driver required by the flashing base.

### Upload Fails

- Lower `upload_speed` in `platformio.ini` to `115200`.
- Confirm the board is seated correctly in the USB base.
- If the base has no automatic download, connect `IO0` to `GND` and reset before upload.
- Remove `IO0` from `GND` and reset after upload.

### WiFi Connection Fails

- Check `WIFI_SSID` and `WIFI_PASSWORD`.
- Use 2.4 GHz WiFi; ESP32-CAM does not support 5 GHz-only networks.
- Move the board closer to the router.
- Watch Serial logs for timeout and status code.

### HTTP Code -1 or Connection Failed

- Confirm Flask is running before the ESP32-CAM posts events.
- Confirm `SERVER_HOST` is the PC LAN IPv4, not `localhost`.
- Confirm ESP32-CAM and PC are on the same WiFi/LAN.
- Check Windows Defender Firewall or other firewall rules for TCP port `5000`.
- Confirm Flask is listening on `0.0.0.0`, not only `127.0.0.1`.

### Brownout or Repeated Resets

- The ESP32-CAM may not be getting enough current.
- Use a stable USB port and cable.
- Avoid powering extra modules from the USB flashing base in this stage.

## 阶段 4 实测记录（2026-06-04）

### 测试结果

- ESP32-CAM 固件成功烧录（ESP32-CAM + USB 烧录底座）。
- Flask 后端运行在电脑上，监听 `0.0.0.0:5000`。
- 手机热点下，Flask 日志连续出现 `POST /api/events HTTP/1.1 201`。
- Dashboard 显示：`state=TEST`、`risk_score=0`、`device_id=labbox_001`、`seq` 持续递增。
- 未接 STM32、未接 UART、未接传感器/执行器。

### 网络经验

- 校园网有认证门户（captive portal），ESP32-CAM 难以自动完成认证，建议使用手机热点测试。
- 手机热点下设备名 `esp32-72AA3C`，MAC `8c:94:df:72:aa:3c`。
- `SERVER_HOST` 必须填电脑在热点/WiFi 下的 IPv4 地址（如 `192.168.x.x`），不能填 `localhost`。

### 串口 Monitor

- 当前串口 Monitor 输出异常/无正常日志。
- 但 HTTP 201 和 Dashboard 显示正常，因此串口 Monitor 不作为阶段 4 阻塞项。
- 后续阶段如需调试 UART，应优先排查串口 Monitor 问题。

## Next Stage

Stage 5 will add STM32 UART input. ESP32-CAM will receive STM32 events and forward them to Flask. STM32 remains the only local real-time safety controller; ESP32-CAM still will not calculate risk or control actuators.
