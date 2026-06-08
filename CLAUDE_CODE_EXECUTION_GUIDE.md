# Claude Code 待执行指南

> 本文件是本地 Claude Code 消费整个文档包的总入口。执行前先读 `CANONICAL_DECISIONS.md`，再按 Task 01→08 逐步推进。

## 1. 文档包地图

| 文件 | 给谁看 | 一句话用途 | 是否含 TODO |
|---|---|---|---|
| `CANONICAL_DECISIONS.md` | 人 + Claude Code | 全局唯一事实源，禁止其他文件冲突 | 是 |
| `README.md` | 人 + Claude Code | 仓库入口和最小成功标准 | 是 |
| `CLAUDE.md` | Claude Code | 本地开发规则、命令边界、安全红线 | 是 |
| `AGENTS.md` | Claude Code | 当前状态表、硬件验证表、禁止事项 | 是 |
| `DEVPLAN.md` | 人 + Claude Code | 两周任务看板与验收门槛 | 是 |
| `docs/00_README.md` | 人 | 设计文档地图 | 是 |
| `docs/01`–`docs/14` | 人 + 报告 | 架构、需求、硬件、软件、AI、测试、答辩正文 | 是 |
| `docs/migration/2026-06-04_平台迁移决策记录.md` | 人 + 答辩 | 平台迁移依据与 legacy 叙事 | 是 |
| `CLAUDE_CODE_TASK_01`–`08` | Claude Code | 可执行任务说明 | 是 |
| `VERTICAL_SLICE_INTEGRATION_CHECKLIST.md` | 人 + Claude Code | 端边垂直切片验收清单 | 是 |
| `CLAUDE_CODE_TODO_INDEX.md` | Claude Code | 全部待办索引 | 自动生成 |
| `CROSS_FILE_CONSISTENCY_REPORT.md` | 人 + Claude Code | 跨文件一致性自检 | 自动生成 |

## 2. 标记约定与检索方式

所有需要本地验证、填值、调查、实测或决策的内容，都以如下格式出现：

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明要做什么
> - 为何 GPT 给不了：原因
> - 期望产物/操作：动作和产出
> - 回填位置：文件与小节
> - 验收：完成标准
```

类型：`VERIFY`、`FILL`、`INVESTIGATE`、`MEASURE`、`DECIDE`。

常用检索：

```bash
grep -RIn "\[CLAUDE_CODE_TODO" .
grep -RIn "\[CLAUDE_CODE_TODO | FILL" .
grep -RIn "\[CLAUDE_CODE_TODO | VERIFY" .
grep -RIn "\[ASSUMPTION" .
```

每完成一个 TODO，必须回填原文档、删除该 TODO 块或改成“已验证”，并在 `tests/` 留记录。

## 3. 执行总顺序

| 顺序 | 入口文档 | 目标 | 通过门槛 |
|---|---|---|---|
| 1 | `CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md` | 迁移分支、legacy 归档、新目录 | 旧 STM32/ESP32 进入 `legacy/`，新目录存在 |
| 2 | `CLAUDE_CODE_TASK_02_wsl_imx6ull_toolchain_ssh.md` | WSL SDK、交叉编译、SSH 到 i.MX6ULL | `hello_imx6ull` 能在板端运行 |
| 3 | `CLAUDE_CODE_TASK_03_imx6ull_gpio_i2c_pwm_bringup.md` | GPIO/I2C/PCA9685/MOS/云台验证 | 空载 PWM、舵机、MOS 负载均有记录 |
| 4 | `CLAUDE_CODE_TASK_04_imx6ull_v4l2_capture.md` | V4L2 抓图 | 板端生成可打开图片，并记录格式/耗时 |
| 5 | `CLAUDE_CODE_TASK_05_opi5_rknn_inference_service.md` | OPi5 AI 服务 | `/health` 与 `/api/infer/vision` 返回 JSON |
| 6 | `CLAUDE_CODE_TASK_06_backend_contract_extension.md` | Flask 扩展 | Dashboard 显示 vision/AI/image 字段且兼容旧事件 |
| 7 | `CLAUDE_CODE_TASK_07_end_edge_vertical_slice.md` | i.MX→OPi5→Flask 完整链路 | Dashboard 显示一条完整端边事件 |
| 8 | `CLAUDE_CODE_TASK_08_pan_tilt巡检演示.md` | 云台扫描演示 | pan/tilt 角度、图片、AI 结果联动显示 |
| 9 | `CLAUDE_CODE_TASK_10_imx6ull_sensor_actuator_p0.md` | P0 传感器/执行器真实化 | 门磁/火焰/MQ-2/PIR 真实 GPIO + 蜂鸣器/RGB 真实驱动；断网断 AI 仍本地 ALARM |
| 10 | `CLAUDE_CODE_TASK_11_imx6ull_p1_oled_relay_soctemp_pump.md` | P1 扩展 | OLED 状态屏 + PCA9685 CH5/CH6 负载 + SoC 温度热健康 + 隔离水箱 |

未通过当前 Task，不进入下一 Task。

## 4. 三机协作模型

| 节点 | 职责 | 部署入口 | 配置占位符 |
|---|---|---|---|
| PC / WSL | 代码编辑、交叉编译、模型转换准备、scp/ssh 部署 | `scripts/build_imx6ull.sh`、`scripts/deploy_imx6ull.sh`、`scripts/deploy_opi5.sh` | `WSL_SDK_PATH=<TODO:FILL>` |
| i.MX6ULL Pro | 本地安全控制、V4L2 抓拍、云台控制、上报事件 | `/opt/edge-ai-safety-monitor/` | `IMX_IP=<TODO:FILL>`、`IMX_USER=<TODO:FILL>` |
| Orange Pi 5 | RKNN 推理服务、可选 Flask 最终部署 | `/opt/edge-ai-safety-monitor/opi5-ai/` | `OPI5_IP=<TODO:FILL>`、`OPI5_USER=<TODO:FILL>` |

Task02 已验证本地 `inventory.yaml`、i.MX6ULL SSH、OPi5 SSH smoke test、hello 编译、scp 与板端运行；
真实值仍只保存在不入库的 `config/inventory.yaml`。证据见 `tests/imx6ull/2026-06-06_toolchain_ssh.md`。
本地 `password` 字段为可选项；部署脚本存在该字段时使用 `sshpass -e`，否则使用常规 SSH key/agent。


## 5. 安全与禁止红线

1. AI 不直接控制执行器。`control_allowed` 必须保持 `false`，只允许 i.MX6ULL 本地状态机执行控制。
2. 不接 220V 强电，不做高压演示。
3. 舵机与水泵等负载独立供电，所有低压 GND 星形共地。
4. MOS 栅极必须有下拉，执行器默认 OFF。
5. 先空载 PWM，再接 MG90；先 LED，再接水泵。
6. 先 mock AI 服务，再接真实 RKNN。
7. 不提交模型、数据库、图片、密钥、真实 IP、WiFi 密码。
8. 不把 legacy 阶段未验收链路写成“已通过”。

## 6. Claude Code 行为准则

- 不编造命令输出；所有命令输出只从本地终端复制。
- 不把“设计完成”写成“硬件已通过”。
- 每次改动后运行最小可行验证。
- 每完成一个 Task，更新 `AGENTS.md` 状态表、`DEVPLAN.md` 看板，并在 `tests/` 新增记录。
- 遇到硬件值、路径、IP、模型名不确定时，新增 `[CLAUDE_CODE_TODO]`，不要猜。
- 任何对 `server/backend/` 的改动必须保持旧 `/api/events` 事件兼容。

## 7. 演示前网络检查

当前优先演示方案：全无线热点（PC + i.MX + OPi5 均连接同一手机热点）。

### 全无线检查（优先）

Windows PowerShell：

```powershell
ipconfig
ping <IMX_WIFI_IP>
ping <OPI5_WIFI_IP>
curl.exe --connect-timeout 5 http://<OPI5_WIFI_IP>:8080/health
curl.exe --connect-timeout 5 http://<PC_WIFI_IP>:5000/api/status/latest
```

i.MX：

```sh
/etc/init.d/S45wifi-client status
ip addr show wlan0
env | grep -i proxy || true
curl -sS --connect-timeout 5 http://<OPI5_WIFI_IP>:8080/health
curl -sS --connect-timeout 5 http://<PC_WIFI_IP>:5000/api/status/latest || true
```

OPi5：

```bash
nmcli dev
lsmod | grep -E '8188|r8188'
env | grep -i proxy || true
curl -sS --connect-timeout 5 http://127.0.0.1:8080/health
```

### 回退方案

若全无线失败：

1. 使用 Windows portproxy 回退（i.MX 有线 → PC:18080 → OPi5 WiFi）。
2. 或插回 OPi5 / i.MX 网线，使用全有线链路（i.MX → OPi5 10.0.1.120:8080）。
3. 不现场改业务代码。

Windows portproxy 回退检查：

```powershell
netsh interface portproxy show v4tov4
curl.exe http://192.168.137.1:18080/health
```

全有线回退检查：

```bash
curl -sS --connect-timeout 5 http://10.0.1.120:8080/health
```

## 8. 每轮回报模板

```text
本轮完成：
- ...

验收结果：
- [ ] 命令：...
- [ ] 输出摘要：...
- [ ] 证据路径：tests/...

已回填 TODO：
- 文件:行号 - 内容

新增 TODO：
- 类型 - 文件:位置 - 原因

风险/阻塞：
- ...

下一步建议：
- ...
```
