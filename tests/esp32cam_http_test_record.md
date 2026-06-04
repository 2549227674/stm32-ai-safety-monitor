# ESP32-CAM HTTP 测试事件验收记录

阶段 4：ESP32-CAM WiFi + HTTP 测试事件。

## 测试目标

验证 ESP32-CAM 能通过 WiFi 连接到 Flask 后端，定时 POST 统一 JSON 测试事件，并在 Dashboard 上正确显示。

## 硬件

| 项目 | 说明 |
|---|---|
| ESP32-CAM 模块 | ESP32-CAM + camera + USB 烧录底座 |
| 连接 | 仅 USB 供电和烧录，未接 STM32、UART、传感器、执行器 |
| 电源 | USB 烧录底座供电 |

## 网络

| 项目 | 说明 |
|---|---|
| 网络类型 | 手机热点（校园网有认证门户，不适合 ESP32-CAM 测试） |
| ESP32 设备名 | esp32-72AA3C |
| ESP32 MAC | 8c:94:df:72:aa:3c |
| Flask 服务器 | 电脑端，监听 0.0.0.0:5000 |
| SERVER_HOST | 电脑在热点下的 IPv4 地址（非 localhost） |

## 接线状态

- ESP32-CAM + USB 烧录底座 → 电脑 USB
- 未连接 STM32
- 未连接 UART（PA2/PA3 未使用）
- 未连接传感器（PIR、门磁、火焰、MQ-2 均未接）
- 未连接执行器（风扇、继电器、水泵、蜂鸣器、RGB 均未接）

## Flask 现象

- `python app.py` 正常启动，监听 `0.0.0.0:5000`。
- Flask 日志连续出现：`POST /api/events HTTP/1.1 201`。
- 事件间隔约 5 秒。

## Dashboard 现象

| 字段 | 值 |
|---|---|
| state | TEST |
| risk_score | 0 |
| device_id | labbox_001 |
| seq | 持续递增 |
| sensors.door | 0 |
| sensors.pir | 0 |
| sensors.flame | 0 |
| sensors.mq2 | 0 |
| actuators.relay | 0 |
| actuators.fan | 0 |
| actuators.pump | 0 |
| actuators.buzzer | 0 |
| actuators.rgb_r | 0 |
| actuators.rgb_g | 0 |
| actuators.rgb_b | 0 |

## 结论

阶段 4 验收通过。ESP32-CAM → Flask HTTP → Dashboard 链路已打通。

## 限制项

1. **未接 STM32**：本次测试不涉及 STM32 ↔ ESP32-CAM UART 通信。
2. **未接 UART**：PA2/PA3 未使用，UART 桥接留到阶段 5。
3. **未抓拍**：ESP32-CAM camera 未启用，无图片上传。
4. **串口 Monitor 异常**：Serial Monitor 输出异常/无正常日志，但不影响 HTTP 和 Dashboard 链路验证。
5. **手机热点**：非正式部署网络，校园网因认证门户不适合测试。
6. **未测试服务器断开重连**：未单独验证服务器断开时 ESP32-CAM 的重试行为。

## 下一步

按 DEVPLAN 顺序，下一步可选：
- **阶段 5**：STM32 → ESP32-CAM UART 桥接（需 STM32 USART2 输出 JSON，ESP32-CAM 转发）
- **或回到 P0-06**：risk_score 风险评分模块实现（不依赖 ESP32-CAM）
