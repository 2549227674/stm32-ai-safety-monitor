# AGENTS.md — 当前状态、硬件验证表与 Agent 规则

## 1. 当前阶段

| 项目 | 状态 |
|---|---|
| 总体阶段 | Task01 仓库迁移与 legacy 归档已完成 |
| 新主线 | i.MX6ULL Pro + Orange Pi 5 + Flask |
| 旧主线 | STM32 + ESP32-CAM 归档为 legacy |
| 首要目标 | 执行 Task02，打通 WSL→i.MX6ULL SDK/SSH/hello 部署 |

## 2. 已知 legacy 状态

| 项目 | 状态 | 说明 |
|---|---|---|
| ESP32-CAM 网络 POST | 历史已验证 | 可向 Flask `/api/events` POST TEST，Flask 返回 201，Dashboard 显示 TEST |
| STM32 PA9 UART 输出 | 历史已验证 | 最新会话记录显示 PA9/USART1_TX 可输出 115200 8N1 JSON |
| STM32→ESP32-CAM GPIO13 最终桥接 | 未最终验收 | PA9 到 GPIO13 物理连接不可靠或引脚位置未确认 |
| 当前 zip 内代码 | 与最新会话状态脱节 | zip 中 STM32 仍为 USART2/huart2，ESP32-CAM 仍默认 UART0 GPIO3/1 |

此表只描述 legacy 阶段，不作为新主线验收。

## 3. 新硬件验证表

| 模块 | 目标 | 初始状态 | 证据路径 | 下一步 |
|---|---|---|---|---|
| i.MX6ULL SSH | PC/WSL 可登录 | 待验证 | `tests/imx6ull/` | Task02 |
| i.MX6ULL SDK | WSL 可交叉编译 | 待验证 | `tests/imx6ull/` | Task02 |
| GPIO 输入 | 门磁/PIR/火焰/按键至少一路 | 待验证 | `tests/imx6ull/` | Task03 |
| I2C | 能发现 PCA9685 或目标 I2C 设备 | 待验证 | `tests/imx6ull/` | Task03 |
| PCA9685 PWM | 50Hz 舵机 PWM | 待验证 | `tests/imx6ull/` | Task03 |
| MG90 云台 | pan/tilt 可控 | 待验证 | `tests/imx6ull/` | Task08 |
| MOS 低压负载 | 默认 OFF 且可控 | 待验证 | `tests/imx6ull/` | Task03 |
| USB 摄像头 | `/dev/video<TODO:VERIFY>` 可抓图 | 待验证 | `tests/imx6ull/` | Task04 |
| OPi5 SSH | PC/WSL 可登录 | 待验证 | `tests/opi5/` | Task05 |
| OPi5 RKNN 服务 | `/health` 和 `/api/infer/vision` 可用 | 待验证 | `tests/opi5/` | Task05 |
| Flask 扩展 | vision/AI/image 显示 | 待实现 | `tests/integration/` | Task06 |
| 端边垂直切片 | i.MX→OPi5→Flask | 待实现 | `tests/integration/` | Task07 |

> **[CLAUDE_CODE_TODO | VERIFY]** 逐项验证新硬件状态表
> - 为何 GPT 给不了：沙箱无法访问 i.MX6ULL、OPi5、摄像头、PCA9685、舵机、MOS 负载。
> - 期望产物/操作：按 Task02–08 执行真实命令和硬件测试，逐项把“待验证”改为“已验证/失败/绕过”。
> - 回填位置：AGENTS.md 第 3 节表格
> - 验收：每项都有 tests 记录、命令输出或照片/波形证据。


## 4. Agent 工作规则

1. 开始前读 `CANONICAL_DECISIONS.md`。
2. 只执行当前 Task 范围内的工作，不跨 Task 大改。
3. 每次写代码先让 mock 链路跑通，再接真实硬件或真实模型。
4. 所有硬件相关结论必须有 `tests/` 证据。
5. 不把未测数据填入报告。
6. 任何新 TODO 都用统一标记块。
7. 完成任务后更新本文件状态表。

## 6. 已完成任务记录

| Task | 状态 | 证据/产物 | 说明 |
|---|---|---|---|
| Task01 仓库迁移与 legacy 归档 | 已完成 | `legacy/2026-stm32-esp32/`、`tests/legacy/2026-06-05_task01_repo_migration_record.md` | 旧 STM32/ESP32-CAM 工程与旧测试记录已归档，新主线目录和文档包已落地。 |

## 5. 禁止事项

- 禁止把旧 STM32/ESP32-CAM 链路写成最终通过。
- 禁止提交 `config/inventory.yaml`、`.env`、模型权重、数据库、抓拍图片。
- 禁止让 AI 直接控制执行器。
- 禁止给舵机/水泵/风扇使用板卡 IO 直接供电。
- 禁止使用真实 220V 负载。
