# CLAUDE.md — Claude Code 工作入口（端边协同迁移版）

> 本文件与 `AGENTS.md` 保持同步，供 Claude Code 恢复后使用。Codex 自动入口仍为
> `AGENTS.md`。二者规则一致；冲突时一律以 `CANONICAL_DECISIONS.md` 为准。

## 0. 开始任何任务前必须先读（按顺序）

1. `CANONICAL_DECISIONS.md` — 全局唯一事实源（项目名 / 四层架构 / 目录树 / 文件名映射 / 标记规范）。
2. `CLAUDE_CODE_EXECUTION_GUIDE.md` — 执行总入口（文档地图 / 三机协作模型 / TODO 检索 / 回报模板）。
3. 当前要执行的 `CLAUDE_CODE_TASK_xx_*.md`。
4. 与任务相关的 `docs/0x_*.md`。
5. 契约权威来源：`docs/07_端边HTTP_JSON_Contract.md`（其他文件只引用，不另造字段）。

> 说明：仓库内执行文档沿用 `CLAUDE_*` 前缀（历史命名），其规则对 Codex 与 Claude Code 同样适用。

## 1. 当前阶段

| 项目 | 状态 |
|---|---|
| 工作目录 | WSL 原生目录 `/home/qbz415/SafetyMonitor`；不要再使用 `/mnt/c/program1/SafetyMonitor` 开发 |
| 当前分支 | `migration/imx6ull-opi5-edge-ai` |
| 最新迁移提交 | `3dfd7a7 task07-c2: fix initd restart flow`；Task07-C2 已完成 |
| 总体阶段 | Task07-C2 已完成：C 版 imx_safetyd + BusyBox init.d 启动管理通过 |
| 新主线 | i.MX6ULL Pro + Orange Pi 5 + Flask |
| 旧主线 | STM32 + ESP32-CAM 归档为 legacy |
| 当前执行器 | 本地 Codex / Claude Code |
| 首要目标 | Task09 最终演示 / Task05-B RKNN demo / 报告与答辩材料，待用户决策 |

## 2. 项目主线与三机角色

主线为 `Edge AI Safety Monitor`：i.MX6ULL Pro 负责 Linux 本地安全控制、V4L2 抓拍与云台；
Orange Pi 5 负责本地 RKNN 推理；Flask + SQLite + Dashboard 保留并扩展。

| 节点 | 角色 | 部署/入口 |
|---|---|---|
| PC / WSL | 代码编辑、交叉编译、模型转换准备、scp/ssh 部署 | `scripts/build_imx6ull.sh`、`scripts/deploy_*.sh` |
| i.MX6ULL Pro | 本地安全控制、V4L2 抓拍、云台、上报事件 | `/opt/edge-ai-safety-monitor/` |
| Orange Pi 5 | RKNN 推理服务（先 mock 后真实） | `/opt/edge-ai-safety-monitor/opi5-ai/` |

真实地址 / SDK 路径 / 端口写在不入库的 `config/inventory.yaml`。

## 3. 任务执行顺序（未过门槛不进下一步）

| 顺序 | 入口文档 | 目标 | 通过门槛 |
|---|---|---|---|
| 1 | `CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md` | 迁移分支、legacy 归档、新目录 | 已完成：旧 STM32/ESP32 进 `legacy/`，新目录存在，无重复同号 docs |
| 2 | `CLAUDE_CODE_TASK_02_wsl_imx6ull_toolchain_ssh.md` | WSL SDK、交叉编译、SSH 到 i.MX6ULL | `hello_imx6ull` 能在板端运行 |
| 3 | `CLAUDE_CODE_TASK_03_imx6ull_gpio_i2c_pwm_bringup.md` | GPIO/I2C/PCA9685/MOS/云台验证 | 空载 PWM、舵机、MOS 负载均有 tests 记录 |
| 4 | `CLAUDE_CODE_TASK_04_imx6ull_v4l2_capture.md` | V4L2 抓图 | 板端生成可打开图片，记录格式/耗时 |
| 5 | `CLAUDE_CODE_TASK_05_opi5_rknn_inference_service.md` | OPi5 AI 服务（先 mock） | `/health` 与 `/api/infer/vision` 返回 JSON |
| 6 | `CLAUDE_CODE_TASK_06_backend_contract_extension.md` | Flask 扩展 | 新旧事件均显示，旧接口兼容 |
| 7 | `CLAUDE_CODE_TASK_07_end_edge_vertical_slice.md` | i.MX→OPi5→Flask 完整链路 | Dashboard 显示一条完整端边事件 |
| 8 | `CLAUDE_CODE_TASK_08_pan_tilt巡检演示.md` | 云台巡检演示 | pan/tilt 角度、图片、AI 结果联动 |

建议路径：Task01 已落地后，先不碰硬件，用 `scripts/run_mock_ai.sh` + `tests/payloads/event_with_ai.json`
跑通“mock 发包 → mock AI → Flask → Dashboard”纯软件垂直切片，作为保底可演示物，再推进 Task02 起的硬件链路。

## 4. 已知 legacy 状态（只描述第一版，不作为新主线验收）

| 项目 | 状态 | 说明 |
|---|---|---|
| ESP32-CAM 网络 POST | 历史已验证 | Flask 返回 201，Dashboard 显示 TEST，seq 递增 |
| STM32 PA9/USART1_TX 输出 JSON | 历史已验证 | 最新会话记录显示 115200 8N1 JSON 可输出 |
| STM32→ESP32-CAM GPIO13 最终桥接 | 未最终验收 | PA9 到 GPIO13 物理连接不可靠或引脚位置未确认 |
| 当前 zip 内代码 | 与最新会话状态脱节 | zip 中 STM32 仍为 USART2/huart2，ESP32-CAM 仍默认 UART0 GPIO3/1 |

## 5. 新硬件验证表（初始全为待验证，禁止预填“已通过”）

| 模块 | 目标 | 初始状态 | 证据路径 | 对应 Task |
|---|---|---|---|---|
| i.MX6ULL SSH | PC/WSL 可登录 | 已验证 | `tests/imx6ull/2026-06-06_toolchain_ssh.md` | Task02 |
| i.MX6ULL SDK | WSL 可交叉编译 | 已验证 | `tests/imx6ull/2026-06-06_toolchain_ssh.md` | Task02 |
| GPIO 输入 | 门磁/PIR/火焰/按键至少一路 | 已验证（PIR / HC-SR501 DO -> J5 D0 / gpio117） | `tests/imx6ull/2026-06-06_gpio_input_probe.md` | Task03 |
| I2C / PCA9685 | 发现地址并输出 50Hz PWM | 地址已验证；空载 PWM 软件准备完成，用户决定跳过波形实测 | `tests/imx6ull/2026-06-06_i2c_pca9685_probe.md`、`tests/imx6ull/2026-06-06_pca9685_pwm_empty_load.md` | Task03 |
| MG90S 云台 | pan/tilt 可控 | 已验证（分时模式：CH0/CH1 各自 1100→1900us 大幅动作通过，无 5V 压降） | `tests/imx6ull/2026-06-06_pca9685_servo_small_angle.md` | Task03/08 |
| MOS 低压负载 | 默认 OFF 且可控 | 本轮跳过（控制端需焊接，MVP 不依赖） | `tests/imx6ull/2026-06-06_mos_skip_decision.md` | Task03（扩展项） |
| USB 摄像头 | `/dev/video1` 可抓图 | 已验证（UVC 1.00, MJPG 640x480, v4l2-ctl 抓帧成功） | `tests/imx6ull/2026-06-06_v4l2_capture.md` | Task04 |
| OPi5 SSH / AI mock 服务 | 登录 + `/health`、`/api/infer/vision` mock 可用 | Task05-A 已验证 | `tests/opi5/2026-06-07_ai_mock_service.md` | Task05-A |
| OPi5 Qwen3-VL 2B 真实 AI | VLM 真实推理 | Task05-B 已通过（rknn+rkllm，中文场景描述，~13s） | `tests/opi5/2026-06-07_qwen3vl_real_ai.md` | Task05-B |
| Flask 扩展 | vision/AI/image 显示 | Task06 已验证 | `tests/integration/2026-06-07_backend_contract_extension.md` | Task06 |
| 端边垂直切片 | i.MX→OPi5→Flask | Task07-C2 已验证（C 版 imx_safetyd：真实 gpio117 空闲读取、FORCE_VERIFY 抓拍、OPi5 mock AI、Flask 上报、offline 降级、spool/flush、loop；BusyBox init.d 启动管理；PIR 未人工触发 raw=1） | `tests/integration/2026-06-07_imx_safetyd_c.md`、`tests/integration/2026-06-07_imx_safetyd_initd.md` | Task07-C2 |

> **[CLAUDE_CODE_TODO | VERIFY]** 逐项验证新硬件状态表
> - 为何 GPT 给不了：沙箱无法访问 i.MX6ULL、OPi5、摄像头、PCA9685、舵机、MOS 负载。
> - 期望产物/操作：按 Task02–08 执行真实命令和硬件测试，逐项把“待验证”改为“已验证/失败/绕过”。
> - 回填位置：AGENTS.md / CLAUDE.md 第 5 节表格
> - 验收：每项都有 tests 记录、命令输出或照片/波形证据。

## 6. 允许做的事

- 重写根目录文档与 `docs/00–14`（保持编号，不新增 `docs/15`）。
- 移动旧 `firmware/stm32`、`firmware/esp32cam` 到 `legacy/2026-stm32-esp32/`。
- 在 `edge/imx6ull-controller/`、`edge/opi5-ai/` 新建 Linux 控制程序与 AI 服务。
- 在 `server/backend/` 增量扩展字段与页面，保持旧 `/api/events` 兼容。
- 在 `common/contracts/`、`scripts/`、`tests/` 维护契约、脚本与真实测试记录。
- 用 `git mv` 归档，不删历史。

## 7. 禁止做的事

| 禁止 | 原因 |
|---|---|
| 把旧 STM32/ESP32-CAM 链路写成最终通过 | 只归档事实，不美化 |
| 继续死磕 STM32→ESP32-CAM UART 桥接 | 已退出新主线 |
| 删除旧代码历史 | legacy 是答辩中的工程迭代证据 |
| 重写 Flask 为另一框架 / 覆盖真实 `app.py`、`database.py` | 现有 Flask 是最可复用资产，只增量改 |
| 让 OPi5 / Flask / AI 直接控制执行器 | 安全闭环必须留在 i.MX6ULL；`control_allowed` 恒为 `false` |
| 提交 `config/inventory.yaml`、`.env`、模型权重、数据库、抓拍图片、真实 IP/密码/密钥 | 入库安全 |
| 给舵机/水泵/风扇用板卡 IO 直接供电；使用 220V 负载 | 硬件安全 |
| 把未实测硬件写成“已通过” | 报告须与真实状态一致 |

## 8. 安全红线（最高优先级）

1. 本地安全闭环优先：无网络 / 无摄像头 / 无 AI 时，i.MX6ULL 仍能依据本地传感器进入安全状态。
2. AI / Dashboard / 上层只返回 `risk_hint`、解释与展示，不直接控制蜂鸣器、RGB、继电器、风扇、水泵。
3. 不接 220V；演示负载用低压直流；舵机与负载独立供电、星形共地；执行器默认 OFF、MOS 栅极下拉。
4. 先空载 PWM 再接 MG90S；先 LED/小风扇再接水泵；先 mock AI 再接真实 RKNN。

## 9. 命令与配置原则

- 真实环境值（IP、用户名、端口、SDK 路径、AI 端口）只从不入库的 `config/inventory.yaml` 读取；
  仓库内只放 `config/inventory.example.yaml`。脚本通过 `scripts/lib_inventory.sh` 取值，不写死。

```bash
cp config/inventory.example.yaml config/inventory.yaml
```

- `config/inventory.yaml` 已在 WSL 本地创建，用于本机真实值，禁止提交。
- 交叉编译与部署：`scripts/build_imx6ull.sh`、`scripts/deploy_imx6ull.sh`、`scripts/deploy_opi5.sh`。
- SDK 环境变量与工具链三元组在 Task02 中确认；若 SDK 无 `environment-setup-*`，则写入 `inventory.yaml` 的 `imx6ull.cc`。

## 10. 端边 Contract 原则

`docs/07_端边HTTP_JSON_Contract.md` 是唯一权威来源，代码字段必须与之同步。
保留旧事件语义 `type/device_id/seq/state/risk_score/need_snap/sensors/actuators`；
新增 `vision/ai_result/image_url/latency_ms/frame_id/pan_tilt` 只能向后兼容（旧后端能忽略或存入 `raw_json`）。
后端扩展落点见 `server/backend/CONTRACT_EXTENSION_NOTES.md`（`raw_json` 已存全量 payload，存储天然兼容）。

## 11. 标记规范（与 CANONICAL 一致）

需要本地验证/填值/调查/实测/决策处，统一用可 grep 的块：

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明
> - 为何 GPT 给不了：原因
> - 期望产物/操作：动作与产出
> - 回填位置：文件与小节
> - 验收：完成标准
```

类型：`VERIFY` `FILL` `INVESTIGATE` `MEASURE` `DECIDE`。默认假设用 `> **[ASSUMPTION]** ...`。
检索全部待办：

```bash
grep -RIn "\[CLAUDE_CODE_TODO" .
```

## 12. 已完成任务记录

| Task | 状态 | 证据/产物 | 说明 |
|---|---|---|---|
| Task01 仓库迁移与 legacy 归档 | 已完成 | `legacy/2026-stm32-esp32/`、`tests/legacy/2026-06-05_task01_repo_migration_record.md`、提交 `a44fd9f` | 旧 STM32/ESP32-CAM 工程与旧测试记录已归档，新主线目录和文档包已落地。 |
| Task02 WSL/i.MX6ULL SDK/SSH/hello 部署 | 已完成 | `tests/imx6ull/2026-06-06_toolchain_ssh.md`、提交 `52c4392` 后续收尾提交 | SDK 已交叉编译 hello，WSL/PC SSH 恢复后板端输出 `hello from imx6ull target`。 |
| Task03 GPIO/I2C/PWM/云台 | 收尾完成 | `tests/imx6ull/2026-06-06_*.md` | GPIO 输入通过；I2C/PCA9685 地址通过；MG90S 分时云台通过；PWM 波形实测跳过；MOS 跳过。 |
| Task04 V4L2 USB 摄像头抓拍 | 已通过 | `tests/imx6ull/2026-06-06_v4l2_capture.md`、`tests/imx6ull/artifacts/2026-06-06_task04_latest.jpg` | USB 免驱摄像头（UVC 1.00）识别成功，v4l2-ctl 抓取 640x480 MJPEG 帧，保存为有效 JPEG 图片。 |
| Task05-A OPi5 AI mock 服务 | 已通过 | `tests/opi5/2026-06-07_ai_mock_service.md` | OPi5 mock AI 服务部署通过，`/health` 与 `/api/infer/vision` 返回符合 docs/07 的 JSON，`control_allowed=false`。 |
| Task06 Flask 契约扩展 | 已通过 | `tests/integration/2026-06-07_backend_contract_extension.md` | 未改 DB schema，`raw_json` 透出 AI/视觉字段，Dashboard 新增 AI 展示区。 |
| Task07-A 端边最小垂直切片 | 已通过 | `tests/integration/2026-06-07_end_edge_vertical_slice.md` | i.MX 抓拍→OPi5 mock AI→Flask 完整链路通过，`control_allowed=false`，延迟 capture=470ms/ai=135ms/post=146ms。 |
| Task07-B imx_safetyd-lite | 已通过 | `tests/integration/2026-06-07_imx_safetyd_lite.md` | Shell 版 MVP 控制端：真实 PIR gpio117 读取、NORMAL/VERIFY、OPi5 降级、Flask spool/flush 通过；`FORCE_VERIFY=1` 用于 VERIFY 测试。 |
| Task08-A 云台分时巡检 | 已通过 | `tests/imx6ull/2026-06-07_pan_tilt_demo.md` | PCA9685+MG90S 三点扫描、抓拍、OPi5 mock AI、Flask 上报通过；`control_allowed=false`。 |
| Task07-C C 版 imx_safetyd | 已通过 | `tests/integration/2026-06-07_imx_safetyd_c.md` | C 版主控程序已交叉编译并部署到 i.MX6ULL，支持 `once/loop/flush`、读取真实 PIR gpio117、NORMAL/VERIFY/FAULT 最小状态机、VERIFY 抓拍、OPi5 mock AI、干净 fallback AI、Flask 上报、pending spool、flush 到 sent、运行日志和 status JSON；当前板端无 `systemctl`，systemd 模板仅入库备用；本轮 PIR 仅测到空闲 raw=0，VERIFY 使用 `FORCE_VERIFY=1`。 |
| Task07-C2 init.d 启动管理 | 已通过 | `tests/integration/2026-06-07_imx_safetyd_initd.md` | BusyBox/SysV `init.d` 适配完成，支持 `start/stop/restart/status`、PID 文件、日志追加、重复启动防护；板端 `/etc/init.d/rcS` 会执行 `S??*`，具备开机自启条件；`/etc/edge-ai-safety-monitor/imx-safetyd.env` 由板端手动创建，未入库。 |

## 13. 后续增强路线（可选，不阻塞 MVP）

| 增强项 | 目标 | 前置条件 | 风险 |
|---|---|---|---|
| Task07-D1 原生 libcurl | 替换 curl 子进程 | 确认 sysroot 有 libcurl | 依赖库缺失 |
| Task07-D2 原生 V4L2 | 替换 v4l2-ctl 子进程 | 无 | mmap 流程复杂 |
| Task07-D3 C 模块化拆分 | 单文件拆分为多模块 | D1/D2 完成后更自然 | 集成回归 |

当前稳定基线：Task07-C C 版 imx_safetyd + v4l2-ctl/curl 子进程。Task07-D 不改变状态机规则和安全边界。

## 14. 完成任务后的固定回填

每完成一个 Task：

1. 更新 `AGENTS.md` / `CLAUDE.md` 第 1 / 第 5 / 第 12 节状态。
2. 更新 `DEVPLAN.md` 看板。
3. 在 `tests/<scope>/YYYY-MM-DD_*.md` 记录真实命令、结果、截图/波形路径。
4. 关闭或改写对应 `[CLAUDE_CODE_TODO]`，并按需重生成 `CLAUDE_CODE_TODO_INDEX.md`。
5. 提交信息：`taskXX: 简短说明`；提交粒度小，不混多个 Task。

## 15. Claude Code 专属提示

- 本文件（`CLAUDE.md`）与 `AGENTS.md` 同步；遇到 `AGENTS.md` 中的规则一律视为同样适用。
- 不确定硬件值/路径/IP/模型名时，新增 `[CLAUDE_CODE_TODO]`，不要猜、不要编造命令输出。
- 每完成一步先跑最小可行验证，再扩大；任何对 `server/backend/` 的改动必须保持旧 `/api/events` 兼容。
- 当前未跟踪的 `reviewed_docs_tmp/`、`temp_docs/`、`tmp/`、`worktree.txt` 是临时材料，不要默认提交。
