# 全局唯一事实与迁移决策 Canonical Decisions

> 生成日期：2026-06-04
> 用途：本文件是全部文档的共同事实源。`README.md`、`docs/00_README.md`、`CLAUDE.md` 与 Task 文档必须与本文件保持一致。

## 0.1 沙箱读取状态

已读取上传的项目 zip，并检查真实仓库的根目录、`docs/00-14`、`server/backend/`、`firmware/stm32/`、`firmware/esp32cam/`、`AGENTS.md`、`DEVPLAN.md` 与测试记录。可确认的仓库事实如下：

| 类别 | 已确认事实 | 处理 |
|---|---|---|
| 根目录文档 | 存在 `README.md`、`CLAUDE.md`、`AGENTS.md`、`DEVPLAN.md`、旧 `CLAUDE_CODE_TASK_01/02/03`、旧垂直切片清单 | 全部由本包重写或新增新任务；旧任务进入 legacy |
| 设计文档 | 存在 `docs/00_README.md` 与 01–14 文档；zip 内中文文件名出现 `#U...` 转义，但编号齐全 | 保留 00–14 编号，使用新中文描述名重写 |
| Flask 后端 | 存在 `server/backend/app.py`、`database.py`、`static/dashboard.js`、`templates/dashboard.html`；当前接口包括 `/`、`/health`、`POST/GET /api/events`、`GET /api/status/latest`、`POST /api/images`、`GET /uploads/<filename>` | 保留路径，在原基础上扩展 vision/AI/image 展示，不推倒重写 |
| 当前事件表 | SQLite `events` 表包含 `timestamp/device_id/seq/type/state/risk_score/need_snap/sensors_json/actuators_json/raw_json/source`；`images` 表包含 `timestamp/device_id/event_id/filename/url/note` | 保留兼容字段，新增字段需兼容旧库或通过迁移脚本处理 |
| STM32 代码 | 当前 zip 里 `main.c` 调用 `MX_USART2_UART_Init()` 与 `AppComm_Init(&huart2)`，`app_comm.c` 发送 `STM32_TEST` JSON | 与最新会话状态不完全一致；归档为 legacy，不继续主线修复 |
| ESP32-CAM 代码 | 当前 zip 里默认使用 `Serial` / UART0，`STM32_UART_RX_PIN=3`、`TX_PIN=1`，负责 UART 行转发到 Flask | 与最新 PA9/GPIO13 方案不完全一致；归档为 legacy |
| 测试记录 | 当前仓库测试记录仍偏向 PA2/UART0 方案；会话总结已更新到 PA9 输出通过、GPIO13 物理桥接未验收 | migration 文档按“仓库与最新会话状态脱节”写实 |

## 0.2 项目名称体系

| 场景 | 统一名称 |
|---|---|
| 报告 / 答辩主标题 | 基于 i.MX6ULL 与 Orange Pi 5 的端边协同本地 AI 多模态安全巡检系统 |
| 简短项目名 | Edge AI Safety Monitor |
| 中文短名 | 端边协同本地 AI 安全巡检系统 |
| 当前仓库短期处理 | 在 `stm32-ai-safety-monitor` 中新建迁移分支，不立即重命名远程仓库 |
| 长期仓库名建议 | `edge-ai-safety-monitor` |

## 0.3 四层逻辑架构

| 层级 | 名称 | 物理节点 | 核心职责 | 不承担的职责 |
|---|---|---|---|---|
| 第一层 | Linux 本地安全控制层 | i.MX6ULL Pro | GPIO/I2C/PWM/MOS、传感器采样、执行器控制、`risk_score`、`safety_fsm`、离线降级 | 不运行重型 AI；不承担 Dashboard 主机 |
| 第二层 | Linux 视觉采集与巡检云台层 | i.MX6ULL Pro | USB 摄像头 V4L2 抓帧、PCA9685 控制 MG90 二维云台、图片缓存与上传 | 不做模型推理；不直接覆盖安全状态机 |
| 第三层 | 本地服务器与可视化层 | PC 初期 / Orange Pi 5 最终 | Flask + SQLite + Dashboard、事件存储、图片展示、AI 结果展示 | 不直接控制蜂鸣器、继电器、风扇、水泵 |
| 第四层 | Orange Pi 5 本地 AI 推理与解释层 | Orange Pi 5 16GB + 128G NVMe | RKNN 视觉推理、人脸/目标/异常识别、可选 rknn-llm 解释 | 不直接控制执行器；只返回 `risk_hint` 与解释 |


## 0.4 统一目录树

```text
edge-ai-safety-monitor/
├── README.md
├── CANONICAL_DECISIONS.md
├── CLAUDE_CODE_EXECUTION_GUIDE.md
├── CLAUDE.md
├── AGENTS.md
├── DEVPLAN.md
├── CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md
├── CLAUDE_CODE_TASK_02_wsl_imx6ull_toolchain_ssh.md
├── CLAUDE_CODE_TASK_03_imx6ull_gpio_i2c_pwm_bringup.md
├── CLAUDE_CODE_TASK_04_imx6ull_v4l2_capture.md
├── CLAUDE_CODE_TASK_05_opi5_rknn_inference_service.md
├── CLAUDE_CODE_TASK_06_backend_contract_extension.md
├── CLAUDE_CODE_TASK_07_end_edge_vertical_slice.md
├── CLAUDE_CODE_TASK_08_pan_tilt巡检演示.md
├── CLAUDE_CODE_TASK_10_imx6ull_sensor_actuator_p0.md
├── VERTICAL_SLICE_INTEGRATION_CHECKLIST.md
├── common/
│   └── contracts/
│       └── README.md
├── docs/
│   ├── 00_README.md
│   ├── 01_项目立项与总体设计_端边协同版.md
│   ├── 02_需求分析与功能优先级_端边协同版.md
│   ├── 03_硬件系统设计与供电安全_iMX6ULL_OPi5.md
│   ├── 04_软件架构与模块划分_Linux边缘控制版.md
│   ├── 05_开发计划与MVP验收标准_Linux_RKNN版.md
│   ├── 06_状态机与联动机制_Linux实现版.md
│   ├── 07_端边HTTP_JSON_Contract.md
│   ├── 08_iMX6ULL视觉采集_云台_Web服务器方案.md
│   ├── 09_OrangePi5_RKNN本地AI推理与解释方案.md
│   ├── 10_设备健康监测与扩展传感器方案.md
│   ├── 11_测试计划与风险矩阵_端边协同版.md
│   ├── 12_材料清单与获取计划_新硬件版.md
│   ├── 13_实训报告与答辩演示方案_迁移版.md
│   ├── 14_上报表内容与答辩摘要_端边AI版.md
│   └── migration/
│       └── 2026-06-04_平台迁移决策记录.md
├── edge/
│   ├── imx6ull-controller/
│   │   ├── src/
│   │   ├── include/
│   │   ├── config/
│   │   ├── systemd/
│   │   └── README.md
│   └── opi5-ai/
│       ├── service/
│       ├── models/
│       ├── scripts/
│       └── README.md
├── server/
│   └── backend/
├── scripts/
├── tests/
│   ├── imx6ull/
│   ├── opi5/
│   ├── integration/
│   └── legacy/
└── legacy/
    └── 2026-stm32-esp32/
```


## 端边数据契约摘要

权威 Contract 在 `docs/07_端边HTTP_JSON_Contract.md`，其他文档只引用，不另造字段。

主链路：

```text
USB 摄像头
  → i.MX6ULL V4L2 抓帧
  → HTTP multipart JPEG + metadata POST 到 Orange Pi 5 `/api/infer/vision`
  → OPi5 返回 objects/faces/risk_hint/summary/action_hint/control_allowed=false
  → i.MX6ULL 本地 `safety_fsm` 融合传感器与 AI `risk_hint`
  → i.MX6ULL POST `/api/events` 到 Flask
  → Dashboard 展示事件、图片、AI 结果
```

保留原事件语义：`type`、`device_id`、`seq`、`state`、`risk_score`、`need_snap`、`sensors`、`actuators`。新增字段：`vision`、`ai_result`、`image_url`、`latency_ms`、`frame_id`、`pan_tilt`。AI 只允许返回 `risk_hint`，不得直接控制执行器。


## 0.5 文件名与编号约定

- 保持 `docs/00`–`docs/14` 数字编号，不新增 `docs/15`。
- `07`、`08`、`09` 的主题已经彻底迁移，允许更新描述名，但必须保留数字前缀。
- 平台迁移决策记录放在 `docs/migration/`，不占用 00–14 编号。
- 所有根目录执行文档服务本地 Claude Code；设计文档服务报告、答辩与总体方案。

| 旧文件/目录 | 新文件/目录 | 处理 |
|---|---|---|
| `README.md` | `README.md` | 重写为端边协同入口 |
| `docs/00_README.md` | `docs/00_README.md` | 保留编号，重写正文 |
| `docs/01_项目立项与总体设计.md` | `docs/01_项目立项与总体设计_端边协同版.md` | 保留编号，更新描述名 |
| `docs/02_需求分析与功能优先级.md` | `docs/02_需求分析与功能优先级_端边协同版.md` | 保留编号，更新描述名 |
| `docs/03_硬件系统设计与供电安全.md` | `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md` | 保留编号，更新描述名 |
| `docs/04_软件架构与模块划分.md` | `docs/04_软件架构与模块划分_Linux边缘控制版.md` | 保留编号，更新描述名 |
| `docs/05_开发计划与MVP验收标准.md` | `docs/05_开发计划与MVP验收标准_Linux_RKNN版.md` | 保留编号，更新描述名 |
| `docs/06_状态机与联动机制.md` | `docs/06_状态机与联动机制_Linux实现版.md` | 保留编号，概念保留、实现重写 |
| `docs/07_UART协议与JSON_Contract.md` | `docs/07_端边HTTP_JSON_Contract.md` | 保留编号，主题改为 HTTP/JSON Contract |
| `docs/08_ESP32CAM抓拍_Web_服务器方案.md` | `docs/08_iMX6ULL视觉采集_云台_Web服务器方案.md` | 保留编号，主题改为 V4L2/云台 |
| `docs/09_云端AI多模态异常解释方案.md` | `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md` | 保留编号，主题改为本地 RKNN |
| `docs/10_MPU6050设备健康监测方案.md` | `docs/10_设备健康监测与扩展传感器方案.md` | 保留编号，更新为 Linux I2C 可选项 |
| `docs/11_测试计划与风险矩阵.md` | `docs/11_测试计划与风险矩阵_端边协同版.md` | 保留编号，测试对象重写 |
| `docs/12_材料清单与获取计划.md` | `docs/12_材料清单与获取计划_新硬件版.md` | 保留编号，材料重写 |
| `docs/13_实训报告与答辩演示方案.md` | `docs/13_实训报告与答辩演示方案_迁移版.md` | 保留编号，叙事更新 |
| `docs/14_上报表内容与答辩摘要.md` | `docs/14_上报表内容与答辩摘要_端边AI版.md` | 保留编号，摘要更新 |
| 无 | `docs/migration/2026-06-04_平台迁移决策记录.md` | 新增，不占用 00–14 编号 |
| `firmware/stm32/` | `legacy/2026-stm32-esp32/stm32/` | 归档，不作为新主线 |
| `firmware/esp32cam/` | `legacy/2026-stm32-esp32/esp32cam/` | 归档，不作为新主线 |
| `server/backend/` | `server/backend/` | 保留并扩展 |
| `tests/` | `tests/legacy/` + 新测试目录 | 历史保留，新链路重建 |


## 全局标记规范

文档中凡遇到需要本地 Claude Code、真实硬件、SSH、SDK、局域网或用户本人确认的内容，统一使用可 grep 的块：

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明要做什么
> - 为何 GPT 给不了：原因
> - 期望产物/操作：要执行什么、产出什么
> - 回填位置：本节哪张表/哪个占位符
> - 验收：怎样算完成
```

类型固定为 `VERIFY`、`FILL`、`INVESTIGATE`、`MEASURE`、`DECIDE`。默认假设统一写成：

```text
> **[ASSUMPTION]** 默认假设 —— 若不成立，对应 [CLAUDE_CODE_TODO] 处理。
```

禁止把未经硬件或命令验证的状态写成“已通过”。


## 0.6 i.MX6ULL P0 传感器/执行器引脚分配（唯一事实源）

P0 范围：门磁、PIR、火焰、MQ-2 四路本地安全输入 + 蜂鸣器、RGB 两路本地报警输出。

设计前提：本项目摄像头走 **USB（/dev/video1 UVC）**，CSI 闲置，因此 J5 的 `CSI_DATA0..7` 释放为最多 8 路 GPIO。**P0 的 8 路信号全部走 J5 直连 GPIO，整条本地安全闭环不依赖 I2C；PCA9685 继续专供舵机 CH0/CH1。** P1/P2（继电器、风扇、温湿度、MPU6050）才使用 PCA9685 空闲通道与 I2C 总线。

| 功能 | 方向 | J5 物理脚 | 原理图网络 | Linux 节点 | 状态 | 关键备注 |
|---|---|---|---|---|---|---|
| PIR | 输入 | D0 | CSI_DATA0 | gpio117 | 已验证 0/1 | 仅测到 raw=0，需人工触发取 raw=1 证据 |
| 门磁 door | 输入 | D1 | CSI_DATA1 | gpio118 | 本轮跳过 | 干簧管/磁簧，passive 触点，3.3V 上拉到 GPIO，**无需分压**；无外部 10k 时裸读不稳定，待补上拉后重测 |
| 火焰 flame DO | 输入 | D2 | CSI_DATA2 | gpio119 | 已验证 | **3.3V 供电**，DO 高有效（raw=1 有火焰），裸读稳定；flame=1 可触发本地 ALARM；传感器灵敏度电位器需后续调整 |
| MQ-2 DO | 输入 | D3 | CSI_DATA3 | gpio120 | 待验 | 加热丝需 5V，DO 为 5V 逻辑，**必须分压**（R1=10k 上、R2=20k 下，5V→3.3V） |
| RGB-R | 输出 | D4 | CSI_DATA4 | gpio121 | 待验 | LED 经 220–330Ω 限流；若 D 线不可用则退到 PCA9685 CH2 |
| 蜂鸣器 buzzer | 输出 | D5 | CSI_DATA5 | gpio122 | 待验 | **必须直连 GPIO + NPN 三极管（如 S8050）驱动**，目的：I2C/PCA9685 失效时仍能鸣响 |
| RGB-G | 输出 | D6 | CSI_DATA6 | gpio123 | 待验 | 同 RGB-R；退路 PCA9685 CH3 |
| RGB-B | 输出 | D7 | CSI_DATA7 | gpio124 | 待验 | 同 RGB-R；退路 PCA9685 CH4 |
| 舵机 pan/tilt | 输出 | I2C(7/8) | I2C1 | /dev/i2c-0 0x40 CH0/CH1 | 已用 | 保持现状，P0 不动 |

> **说明**：P0 模块都在外置面包板/洞洞板上；"直连 GPIO"表示信号线从外置面包板/洞洞板经 J5 接到 i.MX GPIO，不表示模块插在 i.MX 核心板上。P0 本地安全闭环不依赖 I2C / OPi5 / Flask / 网络。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 J5 D1–D7（gpio118–124）在默认 dtb 下可作 GPIO 读写
> - 为何 GPT 给不了：默认 `100ask_imx6ull-14x14.dtb` 中这些脚的 pinmux 是否为 GPIO 需板端确认；仅 gpio117(D0) 实测过。
> - 期望产物/操作：板端 `gpioinfo` 查看 D1–D7 对应 line 是否 `unused`；逐脚 `gpioget`/`gpioset` 验证电平变化；若被 CSI 占用，则改 dtb 释放或把 RGB 退到 PCA9685。
> - 回填位置：本表"状态"列；docs/03 第 3.3；tests/imx6ull。
> - 验收：每路能读到（输入）或控制（输出）明确电平变化。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 5V 模块电平安全
> - 为何 GPT 给不了：i.MX6ULL GPIO 为 3.3V 且不耐 5V，模块实际供电与 DO 摆幅需实测。
> - 期望产物/操作：MQ-2 必接 10k/20k 分压；火焰/PIR 优先 3.3V 供电免分压；用万用表确认进 GPIO 的电平 ≤3.3V 再连线。
> - 回填位置：docs/03 第 3.3；tests/imx6ull。
> - 验收：所有进 GPIO 的信号实测 ≤3.3V。

安全边界（与现有硬约束并列）：
- P0 输入全部直连 GPIO，确保 I2C / OPi5 / Flask / 网络全断时，本地仍能 `flame=1 或 mq2=1 → 直接 ALARM → 驱动蜂鸣器/RGB`。
- 蜂鸣器走直连 GPIO，不经 PCA9685，保证 I2C 失效时报警仍可发声。
- 执行器仍只由 i.MX6ULL 本地状态机驱动；AI/OPi5/Flask 不参与，`control_allowed` 保持 `false`。
- 继电器、风扇、水泵、温湿度、MPU6050 属于 P1/P2，不进入 Task10。

## 0.7 不可违反的硬约束

1. 本地安全闭环优先；无网络、无摄像头、无 AI 时，i.MX6ULL 仍要能依据本地传感器进入安全状态。
2. AI / Dashboard / 上层服务只允许返回 `risk_hint`、解释和展示信息，不直接控制蜂鸣器、RGB、继电器、风扇、水泵。
3. 不接 220V 强电；所有演示负载使用低压直流负载。
4. 舵机与负载独立供电并星形共地；执行器默认 OFF；MOS 栅极下拉；电机类负载加必要保护。
5. 模型权重、数据集、SSH 私钥、局域网 IP、WiFi 密码、SQLite DB、抓拍图片不入库。
6. 未经实测不得写“已通过”；只能写“设计完成 / 待验证 / 预留”。
