# Claude Code 任务 03：ESP32-CAM 最小联网固件（改进版）

> 用法：复制本文件正文给本地 Claude Code / Codex 执行  
> 任务类型：新建 ESP32-CAM 最小联网固件  
> 位置：`firmware/esp32cam/`  
> 当前版本：只做 WiFi + HTTP POST 测试事件，不做拍照，不接 STM32 UART

---

## 0. 审核改进摘要

### 0.1 原任务主要问题

1. 原任务没有 WiFi 重连策略，断网后可能停止上报或长时间阻塞。
2. 原任务没有 HTTP connect/read timeout，服务器不可达时可能卡住主循环。
3. 原任务直接创建 `config.h`，容易把 WiFi 密码提交到 Git。
4. 原任务没有说明 ESP32-CAM 的 LED 引脚差异。AI Thinker ESP32-CAM 常见状态 LED 和闪光灯不是同一个，不能默认使用 GPIO4 闪光灯作为状态灯。
5. 原任务没有说明服务器地址必须是电脑局域网 IP，不能写 `localhost`。
6. 原任务没有统一 `risk_score`、`need_snap`、`type`、RGB 字段，和服务器 Contract 有轻微差异。
7. 原任务没有说明第一版定时测试事件后，如何过渡到 STM32 UART 真实事件。
8. 原任务未要求避免大型 JSON 库；第一版完全可以用固定缓冲和 `snprintf()`，减少依赖。

### 0.2 本版补充内容

1. 增加 `config.example.h` + 本地 `config.h` 的配置方式，要求先创建 `.gitignore`，再创建 `include/config.h`，避免泄露 WiFi 密码。
2. 增加 `ensureWiFiConnected()`、自动重连、HTTP 超时处理。
3. 使用统一事件 JSON Contract。
4. 使用 `snprintf()` 构造小 JSON，不引入 ArduinoJson。
5. 增加局域网、防火墙、服务器 `/health` 检查流程。
6. 增加未来 UART 接收版本的过渡说明。

---

## 1. 项目约束

1. ESP32-CAM 是协处理器，不是主控。
2. ESP32-CAM 不控制任何本地执行器。
3. 第一版只做 WiFi 连接 + HTTP POST 测试事件。
4. 第一版不做 UART 接收。
5. 第一版不做拍照。
6. 第一版不使用 MQTT。
7. 使用 Arduino 框架，优先 PlatformIO；Arduino IDE 可作为备选。
8. 不引入 ArduinoJson 等额外库，使用固定缓冲区构造 JSON。
9. 所有本地安全判断仍由 STM32 完成。

---

## 2. 目标

ESP32-CAM 上电后：

1. 连接 WiFi。
2. 每 5 秒 HTTP POST 一条测试事件到 Flask 服务器 `POST /api/events`。
3. Serial 打印 WiFi、服务器、HTTP 响应码和错误信息。
4. WiFi 断开后自动重连。
5. 服务器不可达时不死锁，等待下一轮重试。
6. 可选状态 LED：连接成功常亮，发送时短闪。若状态 LED 引脚不确定，可禁用 LED。

---

## 3. 目录结构（PlatformIO 优先）

请创建：

```text
firmware/esp32cam/
├── platformio.ini
├── src/
│   └── main.cpp
├── include/
│   ├── config.example.h
│   └── config.h              # 本地生成或由用户复制，不建议提交真实密码
├── .gitignore
└── README.md
```

如果使用 Arduino IDE，可另外提供：

```text
firmware/esp32cam/esp32cam_main/
├── esp32cam_main.ino
└── config.h
```

但本任务优先生成 PlatformIO 版本。

---

## 4. platformio.ini

请写入：

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino
monitor_speed = 115200
upload_speed = 921600

; First version does not use camera or SD card.
; Keep dependencies empty.
lib_deps =
```

说明：如果本地烧录不稳定，用户可把 `upload_speed` 改为 `115200`。

---

## 5. include/config.example.h

请创建：

```cpp
#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// Copy this file to include/config.h and edit local values.

// WiFi
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"

// Flask server on the same LAN.
// Do not use localhost here. Use the PC/server LAN IP, for example 192.168.1.100.
#define SERVER_HOST     "192.168.1.100"
#define SERVER_PORT     5000
#define API_ENDPOINT    "/api/events"
#define HEALTH_ENDPOINT "/health"

// Device
#define DEVICE_ID       "labbox_001"

// Timings
#define POST_INTERVAL_MS        5000UL
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define HTTP_CONNECT_TIMEOUT_MS 3000UL
#define HTTP_READ_TIMEOUT_MS    5000UL

// LED configuration.
// AI Thinker ESP32-CAM often has a small red status LED on GPIO33 active-low.
// GPIO4 is usually the flash LED; do not use GPIO4 as status LED unless explicitly wanted.
#define STATUS_LED_PIN          33
#define STATUS_LED_ACTIVE_LOW   1

#endif /* CONFIG_EXAMPLE_H */
```

---

## 6. include/config.h

创建顺序要求：

1. 先创建 `firmware/esp32cam/.gitignore`，确保其中包含 `include/config.h`。
2. 再创建或复制 `include/config.h`。
3. 如果 `include/config.h` 已经被 Git 暂存，执行 `git rm --cached include/config.h` 后再提交。

请生成一个模板版本，内容可与 `config.example.h` 相同，但 README 中必须说明：

1. 用户需要手动修改 WiFi SSID、密码和服务器局域网 IP。
2. 如果提交到 GitHub，不要提交真实 WiFi 密码。
3. 本目录 `.gitignore` 应忽略 `include/config.h`，但保留 `include/config.example.h`。

---

## 7. firmware/esp32cam/.gitignore

请创建：

```gitignore
.pio/
.vscode/
include/config.h
*.log
.DS_Store
Thumbs.db
```

---

## 8. 统一事件 JSON 格式

ESP32-CAM 第一版发送固定 `state=TEST`，JSON 字段必须与 Flask 任务文档一致，所有 TEST event 的 `risk_score` 固定为 `0`：

```json
{
  "type": "event",
  "device_id": "labbox_001",
  "seq": 1,
  "state": "TEST",
  "risk_score": 0,
  "need_snap": false,
  "sensors": {"door":0,"pir":0,"flame":0,"mq2":0},
  "actuators": {"relay":0,"fan":0,"pump":0,"buzzer":0,"rgb_r":0,"rgb_g":0,"rgb_b":0}
}
```

要求：

1. `seq` 自增。
2. 使用固定 `char json[384]` 或 `char json[512]`。
3. 使用 `snprintf()`，检查返回值。
4. 不引入 ArduinoJson。
5. `Content-Type` 必须为 `application/json`。

---

## 9. src/main.cpp 实现要求

必须包含：

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
```

### 9.1 LED 辅助函数

实现：

```cpp
static void ledWrite(bool on);
static void ledBlinkOnce(uint16_t ms);
```

要求：

1. 若 `STATUS_LED_PIN < 0`，函数直接返回。
2. 支持 active-low。
3. 默认不要使用 GPIO4 闪光灯，避免强光误亮。

### 9.2 WiFi 连接函数

实现：

```cpp
static bool ensureWiFiConnected();
```

要求：

1. 若已连接，直接返回 true。
2. 若未连接，调用 `WiFi.mode(WIFI_STA)`、`WiFi.begin()`。
3. 设置：

```cpp
WiFi.setAutoReconnect(true);
WiFi.persistent(false);
```

4. 最多等待 `WIFI_CONNECT_TIMEOUT_MS`。
5. 连接成功后打印 IP 地址。
6. 失败返回 false，不阻塞主循环。

### 9.3 HTTP POST 函数

实现：

```cpp
static bool postTestEvent();
```

要求：

1. 调用 `ensureWiFiConnected()`。
2. URL 格式：

```cpp
String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + API_ENDPOINT;
```

3. 使用：

```cpp
WiFiClient client;
HTTPClient http;
http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
http.setTimeout(HTTP_READ_TIMEOUT_MS);
http.begin(client, url);
http.addHeader("Content-Type", "application/json");
int code = http.POST((uint8_t*)json, strlen(json));
http.end();
```

4. 打印 HTTP code 和响应 body 前 120 字符。
5. code 在 200~299 之间认为成功。
6. 无论成功失败都释放 HTTPClient。

### 9.4 主循环

要求：

```cpp
void loop()
{
    static uint32_t lastPost = 0;
    uint32_t now = millis();

    if ((now - lastPost) >= POST_INTERVAL_MS)
    {
        lastPost = now;
        postTestEvent();
    }

    delay(20);
}
```

不要在 `loop()` 里长时间阻塞。

---

## 10. README.md 要求

README 至少包含：

1. PlatformIO 编译与烧录命令：

```bash
cd firmware/esp32cam
pio run
pio run -t upload
pio device monitor -b 115200
```

2. 如何创建配置文件：

```bash
cp include/config.example.h include/config.h
```

3. 需要修改：

```text
WIFI_SSID
WIFI_PASSWORD
SERVER_HOST
SERVER_PORT
```

4. Flask 服务器必须先启动：

```text
python server/backend/app.py
```

5. `SERVER_HOST` 必须是电脑/服务器局域网 IP，不是 `localhost`。
6. 常见问题：
   - ESP32-CAM 连不上 WiFi。
   - HTTP code = -1 或连接失败。
   - Windows 防火墙拦截 5000 端口。
   - 服务器只监听 127.0.0.1，ESP32-CAM 访问不到。
   - 烧录失败：检查 IO0 接 GND、供电、串口线、upload_speed。

---

## 11. 后续扩展路径（本次不做）

```text
v2：添加 UART 接收，解析 STM32 发来的 JSON 行，转发到服务器
v3：添加 STM32↔ESP32-CAM ACK/NACK 和心跳
v4：添加拍照功能，收到 need_snap=true 时拍照并上传
v5：Web Dashboard 增加图片显示
v6：云端 AI 异常解释
```

### 11.1 从测试事件切换到真实 UART 数据的过渡方案

1. 当前版本：ESP32-CAM 自己生成 `TEST` 事件。
2. 下一版：STM32 USART2 输出统一 JSON，先用 USB-TTL 验证。
3. 再下一版：ESP32-CAM 按行读取 UART，每收到一行 JSON 就 HTTP POST。
4. ESP32-CAM 不判断传感器真假，不计算风险，不控制执行器。
5. 若 UART 超时无数据，ESP32-CAM 可以每 10 秒上报 heartbeat，但不影响 STM32 本地闭环。

---

## 12. 禁止事项

1. 不要控制继电器、风扇、水泵、蜂鸣器或 RGB。
2. 不要在第一版启用相机。
3. 不要在第一版接 STM32 UART。
4. 不要使用 GPIO4 作为默认状态灯，除非用户明确接受闪光灯亮。
5. 不要把真实 WiFi 密码硬编码到要提交的文件。
6. 不要引入 MQTT、WebSocket 或云平台 SDK。

---

## 13. 完成后请输出

Claude Code / Codex 完成后请输出：

1. 创建的文件列表。
2. 如何配置 WiFi 和服务器地址。
3. 如何编译和烧录。
4. Serial 输出示例。
5. 与 Flask 服务器的联调步骤。
6. 常见故障排查。
7. 已知限制和后续 UART 版本计划。
8. 建议 commit message。

建议 commit message：

```text
feat(esp32cam): add WiFi HTTP test event uploader
```

---

## 14. 验证检查清单

### 14.1 编译与烧录

- [ ] `pio run` 成功。
- [ ] `pio run -t upload` 成功。
- [ ] Serial Monitor 115200 有启动日志。

### 14.2 WiFi

- [ ] 能连接到指定 WiFi。
- [ ] 打印 ESP32-CAM IP。
- [ ] WiFi 密码错误时不会死锁。
- [ ] 断开/重启路由后能重连或持续重试。

### 14.3 HTTP

- [ ] Flask 服务器 `/health` 可从同一局域网访问。
- [ ] 每 5 秒 POST 一条事件。
- [ ] Serial 打印 HTTP 201 或 2xx。
- [ ] Dashboard 能看到 `state=TEST` 事件。
- [ ] 服务器关闭时 ESP32-CAM 打印错误并继续下一轮重试。

### 14.4 数据格式

- [ ] JSON 包含 `type`、`device_id`、`seq`、`state`、`risk_score`、`need_snap`。
- [ ] JSON 包含 `sensors` 和 `actuators`。
- [ ] 服务器返回事件时字段未丢失。
