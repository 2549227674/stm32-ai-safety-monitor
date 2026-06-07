# Legacy 归档：STM32 + ESP32-CAM 第一版（2026）

> 本目录是项目第一版（端云协同）的工程探索归档，**不是失败代码**，而是迁移到
> i.MX6ULL + Orange Pi 5 端边方案前的真实迭代历史。新主线不再修复此处链路。

## 1. 第一版架构

```text
STM32F103C8T6 本地控制
  → ESP32-CAM 抓拍/联网（UART 从机 + WiFi HTTP）
  → Flask + SQLite + Dashboard
  → 云端 AI 解释
```

## 2. 真实验收状态（诚实记录，不美化）

| 链路 | 状态 |
|---|---|
| ESP32-CAM WiFi → Flask `/api/events` POST | 历史已验证（返回 201，Dashboard 显示 TEST，seq 递增） |
| STM32 PA9 / USART1_TX 输出 115200 8N1 JSON | 历史已验证（逻辑分析仪可解码 STM32_TEST JSON） |
| STM32 PA9 → ESP32-CAM GPIO13 物理桥接 → Flask | **未最终验收**（物理连接不可靠 / 引脚位置未确认） |
| PA2 / USART2 方案 | 已排查并放弃 |

## 3. 归档代码与最新会话状态的差异（重要）

归档进本目录的代码停留在较早状态，**与最新排查会话不完全一致**：

- `stm32/` 中 `main.c` 仍调用 `MX_USART2_UART_Init()` 与 `AppComm_Init(&huart2)`（USART2/PA2 方案），
  而最新会话已改用 PA9 / USART1_TX。
- `esp32cam/` 仍默认 `Serial`/UART0（`STM32_UART_RX_PIN=3`、`TX_PIN=1`），
  而最新会话方案为 `HardwareSerial(1)` / GPIO13。

保留差异即可，**不在 legacy 中继续修复**。

## 4. 可被新主线借鉴的思想（概念可复用，实现不复用）

- BSP 分层（bsp_key / bsp_inputs / bsp_outputs / app_comm）→ 映射到 Linux 模块划分。
- 事件 JSON 语义（`state/risk_score/need_snap/sensors/actuators`）→ 演化为 docs/07 端边 HTTP Contract。
- 状态机与 `risk_score` 规则 → 在 i.MX6ULL 用户态重写。
- ESP32-CAM 的 WiFi HTTP POST、超时降级思路 → 参考用于端边 HTTP。

## 5. 历史测试记录

旧测试记录见 `../../tests/legacy/`（由 Task01 迁移）。
