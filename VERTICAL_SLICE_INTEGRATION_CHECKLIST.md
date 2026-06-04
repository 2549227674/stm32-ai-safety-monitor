# 垂直切片联调检查清单

> 建议新增第 6 份文档，放置位置：仓库根目录 `VERTICAL_SLICE_INTEGRATION_CHECKLIST.md`  
> 用途：每做一个“单功能/单传感器/单执行器”垂直切片时使用，防止字段、接线、服务器接口和验收步骤不一致。  
> 适用阶段：P0-05 之后、P1 UART/ESP32-CAM/服务器联调前后。

---

## 1. 使用原则

1. 每次只新增一个变量：一个传感器、一个执行器、一个通信接口或一个页面字段。
2. 每个切片必须先在 STM32 本地验证，再进入 ESP32-CAM/服务器/Web。
3. 测完记录、拍照、commit，然后拔掉非必要模块。
4. ESP32-CAM 只转发、抓拍、上报，不控制本地执行器。
5. 服务器/Web 只展示和记录，不覆盖 STM32 本地判断。

---

## 2. 统一事件 JSON

所有层统一使用：

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

字段统一要求：

| 字段 | STM32 | ESP32-CAM | Flask | Dashboard |
|---|---|---|---|---|
| `type` | 可发送 | 保留/补充 | 存储 | 可不显示 |
| `device_id` | 可不发 | 必须补充 | 存储 | 显示 |
| `seq` | 建议 STM32 生成 | 若无则 ESP32 生成 | 存储 | 显示 |
| `state` | STM32 生成 | 不修改 | 存储 | 显示 |
| `risk_score` | STM32 生成 | 不修改 | 存储 | 显示 |
| `need_snap` | STM32 生成 | 后续用于拍照 | 存储 | 显示 |
| `sensors` | STM32 生成 | 不修改 | 存储 | 显示 |
| `actuators` | STM32 生成 | 不修改 | 存储 | 显示 |

风险分统一使用 0-10 分制：

| 分值 | 含义 |
|---:|---|
| 0 | TEST 或无风险测试事件 |
| 0-2 | NORMAL |
| 3-5 | PRE_ALARM，例如 DOOR 示例为 3 |
| 6-8 | ALARM，例如 SMOKE 示例为 6 |
| >=9 | DANGER，例如 FLAME/DANGER 示例为 9 |

禁止字段漂移：

```text
risk      → 不再作为标准字段，统一改为 risk_score
mq2_do    → 不作为上报字段，统一放到 sensors.mq2
door_open → 不作为上报字段，统一放到 sensors.door
```

---

## 3. STM32 UART 发送检查

### 3.1 推荐 UART

```text
STM32 USART2_TX = PA2
STM32 USART2_RX = PA3
```

注意：USART2 的 PA2/PA3 当前尚未在 `.ioc` 中配置为 USART2，等 Task 04 UART 桥接时再通过 CubeMX 添加，不要在 Task 01 中提前修改。

后续连接 ESP32-CAM：

```text
STM32 PA2 / TX → ESP32-CAM RX
STM32 PA3 / RX ← ESP32-CAM TX
STM32 GND      ↔ ESP32-CAM GND
```

### 3.2 电平和供电

1. STM32F103C8T6 UART 是 3.3V TTL。
2. ESP32-CAM UART 是 3.3V TTL。
3. 两者可直接 UART 连接，但必须共地。
4. 不要把 5V TTL USB-TTL 的 TX 直接接 ESP32-CAM/STM32 RX，除非确认是 3.3V 输出。
5. 不要把 ESP32-CAM 的 5V 供电脚误认为 UART 电平。

### 3.3 TX/RX 交叉

```text
发送端 TX → 接收端 RX
发送端 RX ← 接收端 TX
GND ↔ GND
```

若无数据，优先检查：

1. TX/RX 是否接反或没交叉。
2. GND 是否共地。
3. 波特率是否一致。
4. STM32 是否真的调用了 `HAL_UART_Transmit()`。
5. ESP32-CAM 使用的 UART 引脚是否与相机、microSD 或下载串口冲突。

### 3.4 STM32 JSON 内存约束

STM32F103C8T6 RAM 有限，建议：

```c
char tx_buf[256];
const char *device_id = "labbox_001";
uint8_t need_snap = 0U;
int len = snprintf(tx_buf,
                   sizeof(tx_buf),
                   "{\"type\":\"event\",\"device_id\":\"%s\",\"seq\":%lu,\"state\":\"%s\",\"risk_score\":%u,\"need_snap\":%s,\"sensors\":{\"door\":%u,\"pir\":%u,\"flame\":%u,\"mq2\":%u},\"actuators\":{\"relay\":%u,\"fan\":%u,\"pump\":%u,\"buzzer\":%u,\"rgb_r\":%u,\"rgb_g\":%u,\"rgb_b\":%u}}\n",
                   device_id,
                   seq,
                   state,
                   risk_score,
                   need_snap ? "true" : "false",
                   door,
                   pir,
                   flame,
                   mq2,
                   relay,
                   fan,
                   pump,
                   buzzer,
                   rgb_r,
                   rgb_g,
                   rgb_b);

if ((len > 0) && (len < (int)sizeof(tx_buf)))
{
    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buf, (uint16_t)len, 50U);
}
```

要求：

1. 用 `snprintf()`，不要用 `sprintf()`。
2. 不用动态内存。
3. 不用浮点 `%f`。
4. 固定整数和 0/1 字段。
5. 每帧以 `\n` 结尾。
6. 先通过 USB-TTL 验证输出，再接 ESP32-CAM。

---

## 4. ESP32-CAM HTTP 转发检查

第一阶段：定时 TEST 事件。

- [ ] ESP32-CAM 连接 WiFi。
- [ ] `SERVER_HOST` 是电脑/服务器局域网 IP，不是 `localhost`。
- [ ] Flask 服务器监听 `0.0.0.0:5000`。
- [ ] 防火墙允许 5000 端口。
- [ ] `GET /health` 正常。
- [ ] `POST /api/events` 返回 201。

第二阶段：UART 转发。

- [ ] ESP32-CAM 能按行读取 STM32 JSON。
- [ ] 每行以 `\n` 作为帧边界。
- [ ] ESP32-CAM 不改变 STM32 的 `state`、`risk_score`、`sensors`、`actuators`。
- [ ] 如果 STM32 没有发 `device_id`，ESP32-CAM 可补充 `device_id`。
- [ ] 如果 JSON 明显不完整，丢弃该行并打印错误。

---

## 5. Flask / Dashboard 检查

- [ ] `POST /api/events` 可接收 curl 测试事件。
- [ ] `GET /api/events` 可返回最近事件。
- [ ] `GET /api/status/latest` 无事件时返回 `{ "ok": true, "event": null }`。
- [ ] Dashboard 无事件时不崩溃。
- [ ] Dashboard 能显示 `state`、`risk_score`、传感器、执行器。
- [ ] Dashboard 每 3 秒刷新。
- [ ] SQLite 重启后数据仍存在。

---

## 6. 单切片执行模板

每个切片都按下面格式记录到 `tests/p0_bringup_record.md` 或对应测试日志。

```text
切片名称：PA4 DOOR 垂直切片
日期：YYYY-MM-DD
代码版本/commit：xxxxxxx
硬件接线：
  STM32 PA4 → 门磁一端
  门磁另一端 → GND
  OLED PB6/PB7
  ESP32-CAM 暂不接 / 已接 UART

本地测试：
  PA4→GND：OLED D:0，STATE:IDLE
  PA4 悬空：OLED D:1，STATE:DOOR

通信测试：
  STM32 UART 输出：通过 / 未做
  ESP32-CAM HTTP POST：通过 / 未做
  Flask 存储：通过 / 未做
  Dashboard 显示：通过 / 未做

结论：通过 / 部分通过 / 暂缓
是否拔除模块：是 / 否
遗留问题：
```

---

## 7. 第一个门磁切片验收清单

- [ ] 现场只保留 STM32、OLED、ADKEY、门磁或 PA4 模拟线。
- [ ] DHT11 拔掉。
- [ ] MQ-2 实物拔掉。
- [ ] PIR/火焰非必要时拔掉。
- [ ] PA4 → GND 时 `D:0`。
- [ ] PA4 悬空/门磁远离时 `D:1`。
- [ ] OLED `STATE` 优先级正确：FLAME > MQ2 > DOOR > PIR > IDLE。
- [ ] STM32 发出的 JSON 中 `sensors.door=1`。
- [ ] Flask 事件中 `state=DOOR`。
- [ ] Dashboard 显示 DOOR。
- [ ] 断开 ESP32-CAM 后，STM32 本地显示仍正常。

---

## 8. 常见故障定位矩阵

| 现象 | 优先检查 |
|---|---|
| OLED 显示异常 | I2C PB6/PB7、OLED 地址 0x3C、`app_display` 参数数量 |
| 蜂鸣器上电响 | PB5 默认高电平、`BSP_Output_AllOff()` 是否早调用 |
| PA4 一直 D:1 | 门磁拔掉或 PA4 内部上拉，临时 PA4→GND 模拟门关 |
| ESP32-CAM HTTP -1 | 服务器 IP、防火墙、Flask 是否监听 0.0.0.0、WiFi 是否同网段 |
| ESP32-CAM 烧录失败 | IO0 是否接 GND、5V 供电电流是否足够、串口线 TX/RX/GND、`upload_speed` 是否过高 |
| Dashboard 无数据 | 先 curl POST，确认数据库写入，再查前端 fetch |
| STM32 UART 无数据 | TX/RX 交叉、GND 共地、波特率、USART2 是否初始化 |
| JSON 被服务器拒绝 | `Content-Type: application/json`、字段名 `risk_score`、JSON 是否完整 |
| Keil undefined symbol | 新增 `.c` 未加入 `.uvprojx` |

---

## 9. 每次 commit 前检查

- [ ] 本次只改了一个明确任务。
- [ ] 没有把 WiFi 密码提交到 Git。
- [ ] STM32 新增 `.c` 已加入 Keil 工程。
- [ ] `.ioc` 与引脚变更一致。
- [ ] 文档中的引脚表与代码一致。
- [ ] 测试记录已更新。
- [ ] commit message 能说明阶段，例如：

```text
refactor(stm32): split key input output modules
feat(server): add Flask event API dashboard
feat(esp32cam): add WiFi HTTP test uploader
test(p0): record PA4 door vertical slice
```
