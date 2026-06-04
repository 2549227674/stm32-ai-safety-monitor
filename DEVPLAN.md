# DEVPLAN.md — 垂直切片执行计划（改进版）

> 放置位置：仓库根目录 `DEVPLAN.md`  
> 用途：短期执行看板，记录最近 1~2 周“下一步做什么”  
> 说明：本文件不替代 `docs/00-14`，不新增 `docs/15`。正式架构仍以原设计文档为准，本文件只重排近期执行顺序。
> 本轮统一决策：`risk_score` 暂采用 `0-10` 分制；执行器引脚采用 `PA6/PA7/PB8/PB9` 方案；DHT11 暂缓且不写入验收通过项。

---

## 0. 审核改进摘要

### 0.1 原文档主要问题

1. **时间估算偏乐观**：原计划把 STM32 重构、Git 整理、Flask 服务器放在 1 天内完成，对两周实训和 Keil/硬件联调来说过紧。
2. **P0-05 执行器低压验证位置不够明确**：当前主线已经决定进入 P0-05，不能直接跳到 Web/ESP32-CAM 垂直切片。
3. **UART JSON 字段与 Flask/ESP32 文档不完全一致**：原文 UART 例子使用 `risk`，服务器和 ESP32 文档使用 `risk_score`。
4. **缺少从测试事件切换到真实 UART 数据的过渡路线**：ESP32-CAM 第一版定时发送测试事件，后续如何切换为 STM32 UART 输入需要明确。
5. **缺少 STM32 端 JSON 内存约束说明**：STM32F103C8T6 RAM 较小，不建议用大缓冲、动态分配或浮点格式化。
6. **缺少每个阶段的验收清单**：Claude Code / Codex 执行后容易只完成“文件创建”，但没有闭环验证。

### 0.2 本版补充内容

1. 调整为“冻结现状 → STM32 重构 → P0-05A → Flask → ESP32-CAM → UART 桥接 → 门磁垂直切片”的顺序，明确 P0-05A 继电器低压验证优先于完整垂直切片。
2. 统一事件 JSON 字段为 `risk_score`，统一 `sensors` / `actuators` 嵌套对象，`risk_score` 使用 0-10 分制。
3. 补充 STM32 UART 文本 JSON 的最小规范、缓冲区建议和换行帧边界。
4. 增加每个阶段的验收清单、禁止事项和回滚原则。
5. 保留 P0 → P1 → P2 总体路线，同时允许服务器/Web 骨架并行提前搭建。

---

## 1. 当前项目边界

### 1.1 固定约束

1. STM32F103C8T6 是唯一主控。
2. ESP32-CAM 只负责联网、抓拍、HTTP 上报、Web 图片服务和状态回传，不控制本地执行器。
3. 本地安全判断、风险评分、状态机、OLED 显示、蜂鸣器、RGB、继电器、风扇、水泵必须由 STM32 完成。
4. 即使 ESP32-CAM、WiFi、服务器、Web、AI 全部失效，STM32 本地系统仍必须能独立完成安全巡检和执行器联动演示。
5. STM32 工具链保持 `STM32CubeMX + Keil MDK-ARM + HAL + ST-Link`。
6. 当前 P0 阶段不引入 FreeRTOS。
7. DHT11 当前暂缓，不作为 P0 必验项，不参与 `risk_score` / `safety_fsm`。
8. 当前不做云端 AI；AI 解释留到 P2 增强阶段。
9. 不做 220V 强电演示，不推荐机械臂、小车、PCB 打样或公网云平台强依赖。

### 1.2 当前硬件基线

已完成或基本完成：

| 模块 | 引脚 | 状态 |
|---|---|---|
| PC13 LED | PC13 | 已完成，低电平点亮 |
| AD 四位按键 | PA0 / ADC1_IN0 | 已完成 |
| 蜂鸣器 | PB5 | 已完成，低电平触发，默认高电平关闭 |
| RGB LED | PB0/PB1/PB10 | 已完成 |
| OLED SSD1306 | PB6/PB7 / I2C1 | 已完成 |
| PIR | PB15 | 已完成 |
| MQ-2 DO 软件链路 | PB14 | 软件模拟通过，实物待分压 |
| 火焰传感器 | PB12 | 无火焰状态正常，真实触发待安全补测 |
| 门磁 | PA4 | 已完成，常开型：磁铁靠近 D:0，远离 D:1 |

暂缓或待补测：

| 模块 | 状态 |
|---|---|
| USART1 日志 | USB-TTL 到货后补测 |
| MQ-2 实物 DO/AO | 需要 10kΩ / 20kΩ 分压电阻 |
| DHT11 | 已尝试但暂缓，不继续排错，不作为验收通过项 |
| P0-05A 执行器 | PA6 / RELAY1_CTRL，继电器控制，待验证 |
| P0-05B 执行器 | PA7 / FAN_CTRL，风扇控制，待验证 |
| P0-05C 执行器 | PB8 / PUMP_CTRL，水泵控制，待验证 |
| 备用输出 | PB9 / SPARE_GPIO_OUT，备用，暂不使用 |

---

## 2. 开发方式变更

### 2.1 旧方式问题

```text
传感器 A 接入 → 保留接线 → 传感器 B 接入 → 保留接线 → 传感器 C 接入 → 线越来越多
```

问题：

1. 排线复杂，单一变量难以保证。
2. 任何异常都可能来自线接触、供电、CubeMX、代码、OLED 显示或传感器本体。
3. GPT 代码分散复制到 `main.c` 的 USER CODE 区域，后期难维护。
4. Claude Code / Codex 无法稳定理解工程边界。

### 2.2 新方式原则

```text
最小稳定底座
  → 单功能/单器件接入
  → STM32 本地验证
  → 如需要，再三层打通
  → 记录接线/代码/现象
  → 测完拔掉
  → 下一个切片
```

核心原则：

1. 每次只接一个新增模块。
2. 做完一个功能就交付一个功能。
3. 测完拍照、记录、拔掉，最后集成阶段再接回。
4. STM32 本地安全闭环仍是主线。
5. 服务器/Web 骨架可以并行提前搭建，但不能取代 STM32 本地控制。

---

## 3. 统一事件 JSON Contract

本项目后续所有测试事件、ESP32-CAM 上报和服务器存储统一使用以下字段。

### 3.1 最小事件 JSON

```json
{
  "type": "event",
  "device_id": "labbox_001",
  "seq": 1,
  "state": "DOOR",
  "risk_score": 3,
  "need_snap": false,
  "sensors": {
    "door": 1,
    "pir": 0,
    "flame": 0,
    "mq2": 0
  },
  "actuators": {
    "relay": 0,
    "fan": 0,
    "pump": 0,
    "buzzer": 0,
    "rgb_r": 0,
    "rgb_g": 0,
    "rgb_b": 0
  }
}
```

### 3.2 字段约定

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 固定为 `event`，便于后续扩展 heartbeat/image/ack |
| `device_id` | string | 设备编号，ESP32-CAM 可补充，STM32 可不关心 |
| `seq` | int | 递增序号，STM32 或 ESP32-CAM 生成 |
| `state` | string | `IDLE` / `DOOR` / `PIR` / `SMOKE` / `ALARM` / `TEST` |
| `risk_score` | int | 0~10，`0-2=NORMAL`、`3-5=PRE_ALARM`、`6-8=ALARM`、`>=9=DANGER`；TEST 事件为 0 |
| `need_snap` | bool | 是否建议 ESP32-CAM 抓拍，P1 后再使用 |
| `sensors` | object | 传感器快照，0/1 为主 |
| `actuators` | object | 执行器快照，0/1 为主 |

### 3.3 STM32 UART 文本帧

STM32 → ESP32-CAM 使用 UTF-8/ASCII 文本 JSON，换行结尾：

```text
{"type":"event","seq":1,"state":"DOOR","risk_score":3,"need_snap":false,"sensors":{"door":1,"pir":0,"flame":0,"mq2":0},"actuators":{"relay":0,"fan":0,"pump":0,"buzzer":0,"rgb_r":0,"rgb_g":0,"rgb_b":0}}\n
```

STM32 端建议：

1. 使用固定长度 `char tx_buf[256]` 或 `char tx_buf[384]`。
2. 使用 `snprintf()`，不要使用无边界 `sprintf()`。
3. 不使用动态内存分配。
4. 不使用 `%f` 浮点格式化。
5. 检查 `snprintf()` 返回值，若返回值大于缓冲区长度则丢弃本帧。
6. 每帧以 `\n` 结束，ESP32-CAM 按行读取。

---

## 4. 总体执行顺序

```text
阶段 0：冻结现状和仓库基线
阶段 1：STM32 main.c 模块化重构
阶段 2：P0-05A 继电器低压验证
阶段 3：Flask + SQLite + Dashboard 骨架
阶段 4：ESP32-CAM WiFi + HTTP 测试事件
阶段 5：Task 04：STM32 → ESP32-CAM UART 桥接
阶段 6：第一个垂直切片推荐 PA4 门磁 DOOR
阶段 7：PIR / 火焰 / MQ-2 软件链路切片
阶段 8：P0-05B/C 风扇/水泵低压验证
阶段 9：risk_score + safety_fsm + 本地闭环
阶段 10：集成演示、报告、PPT、答辩材料
```

---

## 5. 阶段 0：冻结现状和仓库基线（0.5 天）

### 目标

确认当前仓库与真实硬件进度一致，避免在重构时丢失已经验证过的内容。

### 操作

1. 记录当前 Core/Inc、Core/Src、`.ioc`、`.uvprojx`。
2. 记录 PA4 门磁已通过、DHT11 暂缓、P0-05 即将开始。
3. 拍摄当前最小接线照片。
4. 确认进入 P0-05 前拔掉 DHT11、MQ-2 实物、火焰、PIR 等非必要模块。
5. PA4 如拔掉门磁且内部上拉，会显示 `D:1/STATE:DOOR`；若希望稳定 `D:0`，临时 PA4 → GND。

### 验收清单

- [ ] 仓库当前分支干净或已 commit。
- [ ] 当前硬件状态已写入 `firmware/stm32/docs/bringup_log.md` 或 `tests/p0_bringup_record.md`。
- [ ] 确认 DHT11 不作为当前任务输入。
- [ ] 确认 P0-05 新增引脚仍未正式加入 CubeMX 或已单独记录：PA6=RELAY1_CTRL、PA7=FAN_CTRL、PB8=PUMP_CTRL、PB9=SPARE_GPIO_OUT。

---

## 6. 阶段 1：STM32 main.c 模块化重构（0.5~1 天）

### 目标

`main.c` 只保留初始化、主循环调度和 OLED 更新；业务逻辑拆到 BSP/App 模块。

### 任务文档

使用：`CLAUDE_CODE_TASK_01_refactor_improved.md`

### 预期新增文件

```text
Core/Inc/bsp_key.h
Core/Src/bsp_key.c
Core/Inc/bsp_inputs.h
Core/Src/bsp_inputs.c
Core/Inc/bsp_outputs.h
Core/Src/bsp_outputs.c
```

### 验收清单

- [ ] Keil 中手动加入 `bsp_key.c`、`bsp_inputs.c`、`bsp_outputs.c`。
- [ ] Rebuild 0 Error。
- [ ] OLED 仍能显示 KEY ADC / KEY ID / F/P/M/D / STATE。
- [ ] DHT11 不再被 `main.c` 调用，OLED 温湿度固定显示 `T:--C H:--%`。
- [ ] 蜂鸣器 PB5 上电不误响。
- [ ] PA4 → GND 时显示 `D:0`；PA4 悬空/断开时显示 `D:1`。

---

## 7. 阶段 2：P0-05A 继电器低压验证（0.5 天）

### 目标

验证 STM32 通过 PA6 控制继电器模块输入端。只做低压输入控制，不接 220V，不接高压负载。

### CubeMX 计划

```text
PA6 → GPIO_Output
User Label → RELAY1_CTRL
Output Level → GPIO_PIN_RESET
Mode → Output Push Pull
Pull-up/Pull-down → No Pull
Speed → Low
```

### 最小接线

```text
PA6 / RELAY1_CTRL → 继电器 IN
继电器 VCC → 5V
继电器 GND → GND
STM32 GND → 继电器 GND / 外部电源 GND
```

### 测试逻辑

```text
K1 按下 → 继电器 ON
K4 按下 → 全部 OFF
OLED 显示 RELAY:0/1（如本阶段尚未改 OLED，可先只看继电器灯/咔哒声）
```

### 验收清单

- [ ] 不接 220V，不接高压负载。
- [ ] 继电器供电不来自 ST-Link 3.3V。
- [ ] STM32 与继电器模块共地。
- [ ] 上电默认 OFF。
- [ ] K1 可触发继电器指示灯或咔哒声。
- [ ] K4 可关闭。
- [ ] 如果继电器模块为低电平触发，已在 `bsp_outputs.h` 中单独调整 `RELAY_ON_LEVEL/RELAY_OFF_LEVEL`。

---

## 8. 阶段 3：Flask + SQLite + Dashboard 骨架（0.5~1 天）

### 目标

搭建第三层最小可运行服务器，不依赖任何硬件。

### 任务文档

使用：`CLAUDE_CODE_TASK_02_flask_server_improved.md`

### 验收清单

- [ ] `python app.py` 可启动。
- [ ] 服务器监听 `0.0.0.0:5000`，ESP32-CAM 可通过局域网访问。
- [ ] `POST /api/events` 可接收统一 JSON。
- [ ] `GET /api/events` 返回最近事件。
- [ ] `GET /api/status/latest` 返回最新状态。
- [ ] 浏览器打开 `/` 能看到 Dashboard。
- [ ] Dashboard 每 3 秒刷新。
- [ ] curl/Postman 测试通过。
- [ ] SQLite 数据库已创建，重启后事件仍存在。

---

## 9. 阶段 4：ESP32-CAM WiFi + HTTP 测试事件（0.5~1 天）

### 目标

ESP32-CAM 作为第二层联网协处理器，先不接 STM32，不拍照，只定时向 Flask 服务器 POST 测试事件。

### 任务文档

使用：`CLAUDE_CODE_TASK_03_esp32cam_improved.md`

### 验收清单

- [ ] ESP32-CAM 可烧录。
- [ ] Serial Monitor 115200 可看到 WiFi 连接状态。
- [ ] ESP32-CAM 与服务器在同一局域网。
- [ ] Flask 服务器防火墙允许 5000 端口。
- [ ] 每 5 秒上报一条 `state=TEST` 事件。
- [ ] Dashboard 可看到 ESP32-CAM 测试事件。
- [ ] 服务器断开时 ESP32-CAM 不死循环卡死，能继续重试。

---

## 10. 阶段 5：Task 04：STM32 → ESP32-CAM UART 桥接（1 天）

### 目标

ESP32-CAM 从“定时发送测试事件”切换为“转发 STM32 UART 事件”。

### UART 接线原则

```text
STM32 PA2 / USART2_TX → ESP32-CAM RX 引脚
STM32 PA3 / USART2_RX ← ESP32-CAM TX 引脚
STM32 GND             ↔ ESP32-CAM GND
```

注意：

1. TX/RX 必须交叉。
2. STM32F103 与 ESP32-CAM 都是 3.3V TTL，不能接 5V UART。
3. 先用 USB-TTL 或串口监视验证 STM32 输出，再接 ESP32-CAM。
4. ESP32-CAM 具体 UART 引脚需根据实际模块和是否使用 microSD/相机确定，不在本阶段盲目固定。

### 过渡路线

```text
v1：curl 发送假事件 → Flask → Dashboard
v2：ESP32-CAM 定时发送 TEST → Flask → Dashboard
v3：STM32 USART2 输出一行 JSON → USB-TTL 验证
v4：ESP32-CAM 按行读取 UART → 原样 HTTP POST
v5：STM32 根据门磁/PIR/火焰/MQ2 发送真实事件
```

### 验收清单

- [ ] STM32 可稳定发出一行以 `\n` 结尾的 JSON。
- [ ] 单帧长度不超过 256 或 384 字节。
- [ ] ESP32-CAM 能按行读取完整 JSON。
- [ ] ESP32-CAM 不修改传感器/执行器判断结果，只转发。
- [ ] 服务器收到的字段与统一 JSON Contract 一致。

---

## 11. 阶段 6：第一个垂直切片推荐 PA4 门磁 DOOR（1 天）

### 选择理由

1. PA4 门磁已实物验证成功。
2. 无预热，不像 PIR。
3. 无分压问题，不像 MQ-2。
4. 无复杂时序，不像 DHT11。
5. 可用 PA4 → GND / 悬空直接模拟。

### 切片流程

```text
PA4 门磁输入
  → STM32 读取 D:0/1
  → OLED 显示 STATE:IDLE/DOOR
  → STM32 UART2 输出统一 JSON
  → ESP32-CAM 读取并 HTTP POST
  → Flask 存 SQLite
  → Dashboard 显示 DOOR 事件
  → 测完拔掉门磁，保留模拟方法
```

### 验收清单

- [ ] PA4 → GND：OLED 显示 `D:0`。
- [ ] PA4 悬空或门磁远离：OLED 显示 `D:1`。
- [ ] `state=DOOR` 事件能从 STM32 到 Dashboard。
- [ ] 断开 WiFi 时，STM32 本地 OLED 和本地判断不受影响。
- [ ] 测试结果记录到 `tests/p0_bringup_record.md`。

---

## 12. 阶段 7：PIR / 火焰 / MQ-2 软件链路切片（1~2 天）

### 推荐顺序

1. PIR PB15：已验证，注意 HC-SR501 预热和保持时间。
2. 火焰 PB12：先用 PB12 → GND 模拟触发，再考虑安全真实触发。
3. MQ-2 PB14：先继续用 PB14 → GND/3.3V 软件模拟，实物等分压电阻到位。

### 验收清单

- [ ] 每次只接一个传感器或只做一个模拟输入。
- [ ] OLED 显示正确。
- [ ] UART JSON 字段正确。
- [ ] Dashboard 事件正确。
- [ ] 测完拔掉并记录。

---

## 13. 阶段 8：P0-05B/C 风扇和水泵低压验证（1 天）

### P0-05B 风扇

```text
PA7 / FAN_CTRL → MOSFET 控制端
风扇电源正极 → 风扇 +
风扇 - → MOSFET 负载端
MOSFET GND → 电源 GND
STM32 GND → 电源 GND
```

验收：K2 控制风扇短时运行，K4 全部关闭。

### P0-05C 水泵

```text
PB8 / PUMP_CTRL → MOSFET 或继电器控制端
K3 按下 → 水泵短时运行
K4 按下 → 全部关闭
```

注意：水泵不要长时间空转。

---

## 14. 阶段 9：risk_score + safety_fsm + 本地闭环（1~2 天）

### 目标

从“调试页状态显示”升级到正式本地安全闭环。

### 建议状态优先级

```text
FLAME > MQ2 > DOOR > PIR > IDLE
```

### 风险评分初版建议

`risk_score` 暂统一为 0-10 分制，不采用 0-100：

| 分值 | 风险等级 |
|---:|---|
| 0-2 | NORMAL |
| 3-5 | PRE_ALARM |
| 6-8 | ALARM |
| >=9 | DANGER |

| 输入 | 分值建议 |
|---|---:|
| TEST | 0 |
| PIR | 2 |
| 门开 / DOOR | 3 |
| MQ-2 烟雾 / SMOKE | 6 |
| 火焰 / FLAME / DANGER | 9 |

限制：总分最大 10。

### 执行器联动建议

| 状态 | 执行器 |
|---|---|
| IDLE | 全部关闭 |
| PIR | OLED 提示，可选 RGB 蓝/绿 |
| DOOR | OLED + 事件上报，可选蜂鸣短鸣 |
| SMOKE | 风扇 + 蜂鸣器 + RGB 黄/红 |
| ALARM | 继电器/水泵/蜂鸣器/RGB，按安全规则短时动作 |

### 验收清单

- [ ] 断开 ESP32-CAM 后，本地闭环仍能运行。
- [ ] OLED 能显示状态和风险评分。
- [ ] 统一 OLED 状态名与 JSON `state` 字段。
- [ ] K4 或指定按键可执行本地消音/关闭动作。
- [ ] 事件上报只是记录，不反向控制执行器。

---

## 15. 阶段 10：集成与答辩准备（2 天）

1. 全部必要模块按最终引脚表接回。
2. 整理线缆和固定硬件。
3. 录制完整演示：正常、门开、PIR、烟雾模拟、火焰模拟、执行器联动、网络断开降级。
4. 截图 Dashboard 和事件列表。
5. 更新实训报告和 PPT。
6. 准备答辩话术：强调 STM32 本地闭环、ESP32-CAM 协处理、AI/Web 是增强层。

---

## 16. 近期任务优先级

当前最优先执行：

```text
1. 执行 Task 01：STM32 代码模块拆分
2. 执行 P0-05A：PA6 继电器输入控制验证
3. 执行 Task 02：Flask 服务器骨架
4. 执行 Task 03：ESP32-CAM 测试事件上报
5. 执行 Task 04：STM32 → ESP32-CAM UART 桥接
6. 做第一个垂直切片：PA4 门磁 DOOR
```

不做：

```text
1. 不继续 DHT11 集成排错
2. 不提前做云端 AI
3. 不让 ESP32-CAM 控制本地执行器
4. 不接 220V 强电
5. 不把 MQ-2 实物直接接入 STM32，除非分压到位
```
