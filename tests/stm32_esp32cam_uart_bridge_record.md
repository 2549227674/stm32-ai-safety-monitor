# STM32 → ESP32-CAM UART 桥接测试记录

阶段 5 / Task 04：STM32 USART2 输出 JSON，ESP32-CAM UART 接收后 HTTP POST 到 Flask `/api/events`。

## 测试目标

验证 STM32 每 3 秒输出一行 `STM32_TEST` JSON，ESP32-CAM 按行接收并原样转发到 Flask，Dashboard 能显示来自 STM32 的事件。

## 当前状态

| 项目 | 状态 | 说明 |
|---|---|---|
| STM32 USART2 | 已配置，待实测 | 用户已通过 CubeMX 启用 PA2=TX、PA3=RX、115200 8N1 |
| STM32 `app_comm` | 代码已准备，待 Keil 编译 | 固定发送 `STM32_TEST`，帧尾 `\n` |
| ESP32-CAM UART bridge | 代码已准备，待 PlatformIO 烧录 | UART 行校验 `{...}` 后 POST `/api/events` |
| Flask / Dashboard | 待联调复测 | 阶段 4 HTTP TEST 已通过，本任务需看 `STM32_TEST` |
| 抓拍 / 图片上传 / AI | 不做 | 本任务明确不启用 |

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
STM32 PA2 / USART2_TX -> ESP32-CAM RX configured by STM32_UART_RX_PIN
STM32 PA3 / USART2_RX <- ESP32-CAM TX configured by STM32_UART_TX_PIN，可选预留
STM32 GND             <-> ESP32-CAM GND
ESP32-CAM             -> USB 烧录底座供电
```

安全限制：

- 不要用 STM32 3.3V 或 ST-Link 3.3V 给 ESP32-CAM 供电。
- 不接 220V 强电。
- ESP32-CAM 不控制本地执行器。
- DHT11 暂缓，不参与本任务。

## 测试步骤

1. 在 Keil 工程中手动加入 `firmware/stm32/SafetyMonitor/Core/Src/app_comm.c`。
2. Keil Rebuild STM32 工程并下载到 STM32F103C8T6。
3. 在 `firmware/esp32cam/include/config.h` 设置 WiFi、`SERVER_HOST`、`ENABLE_PERIODIC_TEST_EVENT=0`、`STM32_UART_RX_PIN`。
4. 执行 `cd firmware/esp32cam && pio run && pio run -t upload`。
5. 启动 Flask：`cd server/backend && python app.py`。
6. 打开 Dashboard。
7. 按接线计划连接 PA2、可选 PA3 和 GND。
8. 观察 Flask 日志是否出现 `POST /api/events HTTP/1.1 201`。
9. 观察 Dashboard 是否出现 `state=STM32_TEST`、`risk_score=0`、`device_id=labbox_001` 且 `seq` 递增。

## 实测结果

待填写。当前仅完成代码和测试计划，尚未完成硬件接线、Keil 编译、PlatformIO 烧录和 Dashboard 验收。

## 未完成项

- UART 桥接硬件联调未完成。
- 尚未确认当前 ESP32-CAM 模块的最佳 RX/TX 引脚。
- 尚未切换为真实门磁/PIR/火焰/MQ2 字段。
- `risk_score` 和 `safety_fsm` 仍未开始。
- ESP32-CAM 抓拍、图片上传和 AI 解释仍未开始。
