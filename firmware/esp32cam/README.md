# ESP32-CAM UART 桥接固件

阶段 5 / Task 04 实现最小 STM32 -> ESP32-CAM UART 桥接。ESP32-CAM 从 STM32 接收一行 JSON，做最小 `{...}` 行校验，然后把该 JSON 原样作为 HTTP body POST 到 Flask `/api/events`。

本任务中 ESP32-CAM 仍然只是 WiFi/HTTP 转发协处理模块。它不抓拍、不上传图片、不运行 AI，也不控制继电器、风扇、水泵、蜂鸣器、RGB LED 或任何本地执行器。

## 功能

- 连接 WiFi。
- 按行读取 STM32 UART JSON 帧，帧尾为 `\n`。
- 丢弃不以 `{` 开头或不以 `}` 结尾的行。
- 将 STM32 JSON 原样 POST 到 Flask `/api/events`。
- 在 Serial 115200 输出 WiFi 状态、ESP32-CAM IP、服务器 URL、HTTP 响应码和错误信息。
- WiFi 断开后自动重连。
- HTTP 操作设置超时，避免服务器不可达时长期阻塞主循环。
- 保留阶段 4 的周期性 `TEST` 事件路径，但默认通过 `ENABLE_PERIODIC_TEST_EVENT=0` 关闭。

## 文件

```text
firmware/esp32cam/
├── platformio.ini
├── src/main.cpp
├── include/config.example.h
├── include/config.h
├── .gitignore
└── README.md
```

`include/config.h` 被 Git 忽略，因为它可能包含真实 WiFi 密码。如果它被误提交，执行：

```bash
git rm --cached firmware/esp32cam/include/config.h
```

## 安装 PlatformIO

推荐方式：

- VS Code + PlatformIO IDE 扩展。
- PlatformIO Core：

```bash
python -m pip install platformio
```

## 配置 WiFi 和 Flask 服务器

如本地还没有配置文件：

```bash
cd firmware/esp32cam
cp include/config.example.h include/config.h
```

编辑 `include/config.h`：

```text
WIFI_SSID
WIFI_PASSWORD
SERVER_HOST
SERVER_PORT
```

`SERVER_HOST` 必须填写电脑/服务器在局域网内的 IPv4 地址，例如 `192.168.1.100`。不要填 `localhost`，因为在 ESP32-CAM 上 `localhost` 指 ESP32-CAM 自己，不是电脑。

Task 04 UART 桥接模式建议保持：

```text
API_ENDPOINT = "/api/events"
HEALTH_ENDPOINT = "/health"
DEVICE_ID = "labbox_001"
ENABLE_PERIODIC_TEST_EVENT = 0
STM32_UART_BAUD = 115200
STM32_UART_RX_PIN = 13
STM32_UART_TX_PIN = 14
```

`STM32_UART_RX_PIN` 是 ESP32-CAM 上连接 STM32 `PA2 / USART2_TX` 的 RX 引脚。默认使用 GPIO13，因为本任务不使用 microSD。如果你的 ESP32-CAM 底板实际暴露了其他更合适的 RX 引脚，只修改本地 `include/config.h`。

只有在复测阶段 4 的 ESP32-CAM 单独 HTTP 测试时，才把 `ENABLE_PERIODIC_TEST_EVENT` 设为 `1`。Task 04 UART 桥接验收时保持 `0`。

## 先启动 Flask

从仓库根目录执行：

```bash
cd server/backend
python app.py
```

在电脑本机确认：

```bash
curl http://localhost:5000/health
```

ESP32-CAM 访问时使用：

```text
http://<your-pc-lan-ip>:5000/health
http://<your-pc-lan-ip>:5000/api/events
```

确认 Flask 监听 `0.0.0.0:5000`，并且防火墙允许局域网访问 TCP 端口 `5000`。

## 构建、烧录和监视

将 ESP32-CAM 插入 USB 烧录底座，并用 USB 连接电脑，然后执行：

```bash
cd firmware/esp32cam
pio run
pio run -t upload
pio device monitor -b 115200
```

如果有多个串口，先列出设备，再指定端口：

```bash
pio device list
pio run -t upload --upload-port COM5
pio device monitor -b 115200 --port COM5
```

Linux/macOS 下端口可能类似 `/dev/ttyUSB0` 或 `/dev/cu.usbserial-*`。

阶段 4 中当前硬件的 Serial Monitor 输出曾异常。因此 Task 04 的验收以 Flask `201` 和 Dashboard 显示 `STM32_TEST` 为准，Serial Monitor 只作为辅助信息。

## UART 接线

```text
STM32 PA2 / USART2_TX -> ESP32-CAM RX configured by STM32_UART_RX_PIN
STM32 PA3 / USART2_RX <- ESP32-CAM TX configured by STM32_UART_TX_PIN，可选预留
STM32 GND             <-> ESP32-CAM GND
ESP32-CAM             -> USB 烧录底座供电
```

不要用 STM32 3.3V 或 ST-Link 3.3V 给 ESP32-CAM 供电。STM32F103 和 ESP32-CAM UART 都是 3.3V TTL，不要在这条链路中接入 5V UART。

## 手动烧录注意事项

多数 ESP32-CAM USB 烧录底座支持自动下载模式。如果上传失败：

- 确认串口设备能被电脑识别。
- 尝试更换 USB 数据线或 USB 口。
- 将 `platformio.ini` 中的 `upload_speed` 从 `921600` 降到 `115200`。
- 确认烧录底座是否支持自动下载。
- 如果没有自动下载，先连接 `IO0` 到 `GND`，按 reset 后上传；上传完成后断开 `IO0` 与 `GND`，再 reset 运行。
- 如果出现 brownout 或反复复位，说明 ESP32-CAM 供电可能不足。

## 串口输出示例

```text
=== ESP32-CAM Safety Monitor UART Bridge ===
[mode] stage 5: STM32 UART JSON -> HTTP event
[mode] no camera, no image upload, no local actuators
[server] http://192.168.1.100:5000/api/events
[health] http://192.168.1.100:5000/health
[uart] rx_pin=13 tx_pin=14 baud=115200
[wifi] disconnected, connecting...
[wifi] connected, ip=192.168.1.88, rssi=-52 dBm
[uart] forward line len=245
[post] url=http://192.168.1.100:5000/api/events
[post] http_code=201 body={"event_id":1,"ok":true,...}
```

## 事件 JSON

Task 04 中 STM32 每 3 秒发送以下结构，ESP32-CAM 不修改 body，只负责转发：

```json
{
  "type": "event",
  "device_id": "labbox_001",
  "seq": 1,
  "state": "STM32_TEST",
  "risk_score": 0,
  "need_snap": false,
  "sensors": {"door": 0, "pir": 0, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 0}
}
```

## 验收步骤

1. 启动 Flask 后端并打开 Dashboard。
2. 在 `include/config.h` 中配置 WiFi 和电脑局域网 IP。
3. 执行 `pio run` 构建 ESP32-CAM 固件。
4. 将 ESP32-CAM 插入 USB 烧录底座并连接 USB。
5. 执行 `pio run -t upload` 烧录。
6. 将 STM32 `PA2 / USART2_TX` 接到配置的 ESP32-CAM RX 引脚，并连接公共 GND。
7. ESP32-CAM 使用 USB 烧录底座供电，不要从 STM32 取电。
8. Keil 工程加入 `Core/Src/app_comm.c` 后，编译并下载 STM32 固件。
9. 确认 Flask 日志出现 `POST /api/events HTTP/1.1 201`。
10. 确认 Dashboard 显示重复的 `state=STM32_TEST`、`risk_score=0`、`device_id=labbox_001` 事件，且 `seq` 由 STM32 递增。

## 故障排查

### 找不到串口

- 检查 Windows 设备管理器或执行 `pio device list`。
- 重新插拔 USB 烧录底座。
- 更换 USB 数据线。
- 安装烧录底座所需的 USB 串口驱动。

### 上传失败

- 将 `platformio.ini` 中的 `upload_speed` 降到 `115200`。
- 确认 ESP32-CAM 在烧录底座中插牢。
- 如果底座没有自动下载，上传前连接 `IO0` 到 `GND` 并 reset。
- 上传完成后断开 `IO0` 与 `GND`，再 reset 运行。

### WiFi 连接失败

- 检查 `WIFI_SSID` 和 `WIFI_PASSWORD`。
- 使用 2.4 GHz WiFi，ESP32-CAM 不支持 5 GHz-only 网络。
- 将模块靠近路由器或热点。
- 观察 Serial 日志中的超时和状态码。

### HTTP Code -1 或连接失败

- 确认 Flask 已启动。
- 确认 `SERVER_HOST` 是电脑局域网 IPv4，不是 `localhost`。
- 确认 ESP32-CAM 和电脑在同一 WiFi/LAN。
- 检查 Windows Defender Firewall 或其他防火墙是否允许 TCP 端口 `5000`。
- 确认 Flask 监听 `0.0.0.0`，不是只监听 `127.0.0.1`。

### Dashboard 没有 STM32_TEST

- 确认 UART 桥接模式下 `ENABLE_PERIODIC_TEST_EVENT` 为 `0`。
- 确认 STM32 的 `app_comm.c` 已加入 Keil 工程并重新编译下载。
- 确认 STM32 USART2 为 115200 8N1，且 `MX_USART2_UART_Init()` 已调用。
- 确认 STM32 PA2 接到 `STM32_UART_RX_PIN` 指定的 ESP32-CAM RX 引脚。
- 确认 STM32 GND 和 ESP32-CAM GND 已连接。
- 确认 STM32 发送帧以 `\n` 结尾。

### Brownout 或反复复位

- ESP32-CAM 可能供电不足。
- 使用稳定 USB 口和数据线。
- 本阶段不要从 USB 烧录底座额外给其他模块供电。

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

## 下一阶段

Task 04 硬件联调通过后，下一轮再把固定 `STM32_TEST` 字段替换为真实 STM32 传感器和状态字段。后续仍然必须保持 `risk_score`、`safety_fsm` 和本地执行器控制在 STM32 侧。
