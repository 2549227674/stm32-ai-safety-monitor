# STM32 → ESP32-CAM UART 桥接测试记录

阶段 5 / Task 04：STM32 USART1 输出 JSON，ESP32-CAM UART 接收后 HTTP POST 到 Flask `/api/events`。

## 测试目标

验证 STM32 每 1 秒输出一行 `STM32_TEST` JSON，ESP32-CAM 按行接收并原样转发到 Flask，Dashboard 能显示来自 STM32 的事件。

## 当前状态

| 项目 | 状态 | 说明 |
|---|---|---|
| STM32 USART1 | PA9 输出已验证 | PA9=USART1_TX，115200 8N1；逻辑分析仪已验证可输出 `STM32_TEST` JSON |
| STM32 `app_comm` | 代码已准备，待 Keil 编译 | 固定发送 `STM32_TEST`，帧尾 `\n` |
| ESP32-CAM UART bridge | 改为 UART1/GPIO13，待联调 | 使用 `HardwareSerial(1)`，GPIO13/IO13 接收 STM32 JSON，GPIO14 TX 预留不接 |
| Flask / Dashboard | 网络侧已验证，UART 桥接待验证 | `ENABLE_PERIODIC_TEST_EVENT=1` 时 ESP32-CAM 可 POST `TEST` 到 Flask，Dashboard 正常显示；本任务需看 `STM32_TEST` |
| 抓拍 / 图片上传 / AI | 不做 | 本任务明确不启用 |
| PA2 / USART2 | 暂时放弃 | 逻辑分析仪未得到稳定 115200 UART 波形，阶段 5 改用 PA9/USART1_TX |
| U0R / RX / GPIO3 | 暂时不采用 | 疑似 UART0 与下载串口/日志串口/USB-UART 复用冲突 |

## STM32 预期 JSON

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

实际 UART 帧为单行 JSON，并以 `\n` 结尾。

## 接线计划

```text
STM32 PA9 / USART1_TX -> ESP32-CAM GPIO13 / IO13
STM32 GND             <-> ESP32-CAM GND
ESP32-CAM             -> USB 烧录底座供电，或外部 5V 降压板供电
```

安全限制：

- 不要用 STM32 3.3V 或 ST-Link 3.3V 给 ESP32-CAM 供电。
- 如果使用外部 5V 降压板给 ESP32-CAM 供电，不要同时接 USB 烧录底座供电。
- GPIO14 / UART1 TX 预留，本轮不接。
- 烧录 ESP32-CAM 时可以先断开 `STM32 PA9 -> ESP32-CAM GPIO13/IO13`，烧录完成后再接回。
- 不接 220V 强电。
- ESP32-CAM 不控制本地执行器。
- DHT11 暂缓，不参与本任务。

## 测试步骤

1. 确认 Keil 工程中已加入 `firmware/stm32/SafetyMonitor/Core/Src/app_comm.c`。
2. Keil Rebuild STM32 工程并下载到 STM32F103C8T6。
3. 烧录 ESP32-CAM UART1/GPIO13 桥接固件，并保持 `ENABLE_PERIODIC_TEST_EVENT=0`。
4. 启动 Flask：`cd server/backend && python app.py`。
5. 打开 Dashboard。
6. 按接线计划连接 PA9 到 GPIO13/IO13，并连接 GND。
7. 观察 Flask 日志是否出现 `POST /api/events HTTP/1.1 201`。
8. 观察 Dashboard 是否出现 `state=STM32_TEST`、`risk_score=0`、`device_id=labbox_001` 且 `seq` 递增。

## 实测结果

待填写。已完成：STM32 PA9/USART1_TX 已通过逻辑分析仪验证，可输出 115200 8N1 `STM32_TEST` JSON；`ENABLE_PERIODIC_TEST_EVENT=1` 时 ESP32-CAM 可 POST `TEST` 到 Flask，Dashboard 正常显示。PA2/USART2 因逻辑分析仪未得到稳定 115200 UART 波形，阶段 5 暂时放弃；U0R/RX/GPIO3 因疑似 UART0 复用冲突暂不采用。最终桥接仍待验证，验收标准是 STM32 PA9 -> ESP32-CAM GPIO13 -> Flask -> Dashboard 显示 `state=STM32_TEST` 且 `seq` 由 STM32 递增。

## 未完成项

- UART 桥接硬件联调未完成。
- 尚未验证 ESP32-CAM `GPIO13/IO13` 能稳定接收 STM32 PA9 输出并转发到 Flask/Dashboard。
- 尚未切换为真实门磁/PIR/火焰/MQ2 字段。
- `risk_score` 和 `safety_fsm` 仍未开始。
- ESP32-CAM 抓拍、图片上传和 AI 解释仍未开始。
