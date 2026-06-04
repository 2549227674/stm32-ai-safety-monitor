# Safety Monitor Backend

Flask + SQLite backend for the Safety Monitor course project. This stage only provides the server API, image upload placeholder, and native Web Dashboard. STM32 remains the only local controller; ESP32-CAM will later use HTTP POST for networking, capture, and upload.

## Requirements

- Python 3.10+ recommended
- Flask 3.x
- SQLite from the Python standard library

## Setup

```bash
cd server/backend
python -m venv .venv
```

Activate the virtual environment:

```bash
# Git Bash / Linux / macOS
source .venv/bin/activate

# Windows PowerShell
.venv\Scripts\Activate.ps1
```

Install dependencies:

```bash
pip install -r requirements.txt
```

Start the server:

```bash
python app.py
```

The development server listens on `0.0.0.0:5000` so other devices on the same LAN can reach it. Do not expose this Flask development server to the public internet.

## Health Check

```bash
curl http://localhost:5000/health
```

Expected response:

```json
{"ok":true,"service":"safety-monitor-backend","version":"0.1.0"}
```

## Post a Test Event

```bash
curl -X POST http://localhost:5000/api/events \
  -H "Content-Type: application/json" \
  -d '{"type":"event","device_id":"labbox_001","seq":1,"state":"DOOR","risk_score":3,"need_snap":false,"sensors":{"door":1,"pir":0,"flame":0,"mq2":0},"actuators":{"relay":0,"fan":0,"pump":0,"buzzer":0,"rgb_r":0,"rgb_g":0,"rgb_b":0}}'
```

Read events:

```bash
curl http://localhost:5000/api/events
curl http://localhost:5000/api/status/latest
```

## Dashboard

Open:

```text
http://localhost:5000/
```

The dashboard shows current `state`, `risk_score`, `device_id`, `seq`, sensor status, actuator status, and the latest 20 events. It refreshes every 3 seconds and displays an API error if the backend is unreachable or returns invalid data.

## LAN Access

ESP32-CAM must not use `localhost`, because `localhost` would point to the ESP32-CAM itself. Configure it with the computer LAN IP and port:

```text
SERVER_HOST = "192.168.x.x"
SERVER_PORT = 5000
POST http://192.168.x.x:5000/api/events
```

Find the LAN IP:

```bash
# Windows
ipconfig

# Linux / macOS
ip addr
```

Use the IPv4 address on the same WiFi or wired LAN as the ESP32-CAM.

## Firewall Troubleshooting

- Confirm the PC and ESP32-CAM are on the same network.
- Confirm Flask is started with `host="0.0.0.0"`.
- Allow inbound TCP port `5000` in Windows Defender Firewall or the active firewall.
- If port `5000` is occupied, stop the other process or change the Flask port.
- If the dashboard has no data, POST a test event with `curl` first.

## CORS Note

CORS is enabled only for `/api/*` during development. ESP32-CAM direct HTTP POST requests are not browser requests and are not blocked by browser CORS. CORS mainly helps when testing from a browser page served from another port.

## ESP32-CAM Integration Later

The ESP32-CAM should POST event JSON to `/api/events` and upload captured images to `/api/images` with `multipart/form-data`. It should still not control relays, pumps, fans, buzzers, or LEDs; STM32 remains the only local safety controller.
