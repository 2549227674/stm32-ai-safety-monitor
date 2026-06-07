# CLAUDE_CODE_TODO_INDEX.md — 全部待办总表

生成时间：2026-06-07T12:14:31+0800

本文件由文档包中的 `[CLAUDE_CODE_TODO | 类型]` 标记汇总生成。完成待办后，请回填原文件并重新生成本索引。

## ENHANCE

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `DEVPLAN.md` | Task07-D1：原生 libcurl HTTP client | 用 libcurl API 替换 curl 子进程，封装 GET/POST/multipart/timeout/错误码。 | 编译通过，/health GET、/api/infer/vision POST、/api/events POST 结果与 curl 子进程一致。 |
| `DEVPLAN.md` | Task07-D2：原生 V4L2 抓拍 | 用 open/ioctl/mmap 访问 /dev/video1，替换 v4l2-ctl。 | 成功抓取 MJPG 640x480 JPEG，与 v4l2-ctl 抓拍文件对照一致。 |
| `DEVPLAN.md` | Task07-D3：C 版 imx_safetyd 模块化拆分 | 拆分为 gpio/camera/http/event/spool/fsm/main 等模块。 | Task07-C 已通过行为不退化（NORMAL/VERIFY/offline/spool/flush/loop/status JSON）。 |

## DECIDE

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `DEVPLAN.md:70` | 确认最终 Dashboard 部署位置 | 用户拍板：最终使用 PC Flask，还是迁移 Flask 到 OPi5。 | 最终演示脚本与部署命令只保留一种主方案。 |
| `README.md:71` | 确认最终演示是否必须完全脱离 PC | 用户决定最终演示形态：PC 运行 Flask，或 OPi5 同时运行 Flask + AI。 | 明确写入“最终演示部署位置”，并更新相关 Task。 |
| `docs/05_开发计划与MVP验收标准_Linux_RKNN版.md:60` | 确认 MVP 是否允许 AI 服务先 mock 再替换 RKNN | 用户确认答辩底线：mock 只作为联调手段，最终是否必须跑真 RKNN。 | MVP 标准与最终报告措辞一致。 |

## FILL

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `README.md:78` | 填写三机局域网地址与 SSH 用户 | 创建 `config/inventory.example.yaml`，本地复制为不入库的 `inventory.yaml` 后填入 `IMX_IP=<TODO:FILL>`、`OPI5_IP=<TODO:FILL>`、`BACKEND_HOST=<TODO:FILL>`。 | `ssh <USER>@<IMX_IP>` 与 `ssh <USER>@<OPI5_IP>` 均能登录；真实值只保存在本地。 |
| `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md:46` | 填写 i.MX6ULL GPIO chip/line 与有效电平 | 在板端运行 `gpiodetect`、`gpioinfo`，结合原理图/扩展板丝印填写表格。 | 每一路能用 `gpioget` 或测试程序读到高低变化。 |

## VERIFY (P1)

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `CANONICAL_DECISIONS.md` 0.7 | ~~I2C 0x40/0x3C 共存~~ | 已完成：i2cdetect 同时看到 0x40/0x3C，oled_test 刷屏不影响 PCA9685 RGB | `tests/imx6ull/2026-06-07_p1_oled_status_screen.md` |
| `CANONICAL_DECISIONS.md` 0.7 | SoC 温度 thermal/hwmon 路径 | 板端读取 thermal_zone0 或 hwmon 路径，记录温度数值 | 能稳定读到约 40–60°C |
| `CANONICAL_DECISIONS.md` 0.7 | CH5 继电器默认 OFF 与 3.3V 触发 | 先空载确认默认不吸合；再测 3.3V 触发 | 默认 OFF，程序控制可吸合/释放 |
| `CANONICAL_DECISIONS.md` 0.7 | CH6 水泵 MOS 默认 OFF 与隔离水箱安全 | 先空泵点动；再装水闭环测试 | 默认 OFF、ALARM 喷淋、解除停泵、无漏电 |

## VERIFY

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `AGENTS.md:83` | 逐项验证新硬件状态表 | 按 Task02–10 执行真实命令和硬件测试，逐项把”待验证”改为”已验证/失败/绕过”。 | 每项都有 tests 记录、命令输出或照片/波形证据。 |
| `CANONICAL_DECISIONS.md` | 确认 J5 D1–D7（gpio118–124）在默认 dtb 下可作 GPIO 读写 | 板端 `gpioinfo` 查看 D1–D7 对应 line 是否 `unused`；逐脚 `gpioget`/`gpioset` 验证电平变化。 | 每路能读到（输入）或控制（输出）明确电平变化。 |
| `CANONICAL_DECISIONS.md` | 确认 5V 模块电平安全 | MQ-2 必接 10k/20k 分压；火焰/PIR 优先 3.3V 供电免分压；万用表确认进 GPIO 电平 ≤3.3V。 | 所有进 GPIO 的信号实测 ≤3.3V。 |
| `CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md:11` | 确认当前仓库工作区是否干净 | 运行 `git status` 并决定是否先提交/暂存/备份。 | 工作区无意外未提交修改，或已明确保留。 |
| `common/contracts/README.md:19` | 从 docs/07 生成或同步 JSON Schema | Task06/07 后根据实际 API 生成 `event_v1.schema.json` 和 `infer_v1.schema.json`。 | schema 能验证测试 payload。 |
| `docs/00_README.md:72` | 迁移分支落地后核对 docs/00 链接 | 解压本包到迁移分支后，运行 `find docs -maxdepth 2 -type f` 并核对文件名。 | 所有链接路径存在，无 `docs/15`。 |
| `docs/02_需求分析与功能优先级_端边协同版.md:26` | 确认 i.MX6ULL 上 USB 摄像头设备节点和格式 | 运行 `v4l2-ctl --list-devices` 和 `--list-formats-ext`，记录 `/dev/videoX` 与 MJPEG/YUYV 支持情况。 | 测试记录中包含设备节点、分辨率、像素格式、抓帧耗时。 |
| `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md:64` | 确认 PCA9685 I2C 地址和电平兼容性 | 运行 `i2cdetect -y <bus>`，用万用表确认 VCC/V+，必要时使用电平转换。 | `i2cdetect` 能看到设备地址，空载 PWM 有逻辑分析仪证据。 |
| `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md:81` | 确认 MOS 模块接线和默认 OFF | 先不接负载，测 GPIO 默认电平；再接 LED 验证；最后再接泵。 | 上电未运行程序时负载不动作，程序控制后可开关。 |
| `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md:96` | 确认 Orange Pi 5 系统、供电和网络状态 | 记录系统版本、Python 版本、rknn 依赖、IP、SSH 可用性；真实 IP 不入库。 | `ssh OPI5_USER@OPI5_IP`、`python3 --version`、`curl /health` 均有记录。 |
| `docs/08_iMX6ULL视觉采集_云台_Web服务器方案.md:26` | 确认 V4L2 工具和设备节点 | 在板端安装/确认 v4l2 工具，运行 list-devices/list-formats-ext。 | 记录设备节点、格式、分辨率、命令输出。 |
| `docs/10_设备健康监测与扩展传感器方案.md:22` | 确认 MPU6050 是否接入新硬件清单 | 用户决定是否纳入加分项；若纳入，确认 I2C 总线、地址、电源电压。 | 明确写成“纳入/不纳入最终演示”。 |
| `docs/14_上报表内容与答辩摘要_端边AI版.md:52` | 最终提交前回填真实完成情况 | 根据 Task01–08 测试记录回填已完成项、未完成项和原因。 | 上报表内容与 tests 证据一致。 |
| `docs/migration/2026-06-04_平台迁移决策记录.md:61` | 迁移归档后核对 legacy 文件完整性 | 本地执行 Task01 后确认旧代码、旧文档、旧测试记录都进入 `legacy/2026-stm32-esp32/`。 | `git status` 显示移动清晰，旧代码可追溯。 |

## INVESTIGATE

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `CLAUDE_CODE_TASK_05_opi5_rknn_inference_service.md:11` | 确认 OPi5 系统与 RKNN 依赖 | 运行系统版本、Python 版本、已有 RKNN demo 盘点。 | 形成 OPi5 资产清单和首选 demo。 |
| `DEVPLAN.md:63` | ~~Task05-B 盘点并验证 OPi5 与 PC 上已有 RKNN 仓库~~ | 已完成：Qwen3-VL 2B 通过 rknn-llm C++ demo 部署，真实推理通过。 | `tests/opi5/2026-06-07_qwen3vl_real_ai.md` |
| `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md:25` | 盘点 PC 和 OPi5 上已有 RKNN 仓库与可运行 demo | Claude Code 在本机与 OPi5 上列目录、运行已有 demo，选择最小可接入模型。 | 产出 RKNN 资产清单：模型名、输入尺寸、类别、运行命令、是否通过。 |
| `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md:85` | 确认 rknn-llm 在 OPi5 上是否能稳定运行 | 优先不纳入 MVP；若有余力，运行最小 demo，记录延迟和内存。 | 能在 OPi5 上生成简短文本，且不影响视觉服务稳定性。 |
| `edge/opi5-ai/models/README.md:9` | 填写 OPi5 可用 RKNN 模型清单 | 运行模型盘点命令，记录可用 `.rknn`、demo、依赖版本。 | 至少选出一个可用于 Task05 的 demo 模型。 |

## MEASURE (P1)

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `CANONICAL_DECISIONS.md` 0.7 | 低压负载供电预算：继电器、水泵、电源温升、压降 | 万用表测各路电压电流，记录负载启动时是否掉压。 | 表中写入实测电压/电流/温升。 |

## MEASURE

| 文件:行 | 待办 | 期望产物/操作 | 验收 |
|---|---|---|---|
| `VERTICAL_SLICE_INTEGRATION_CHECKLIST.md:53` | 填写垂直切片实际验收结果 | Task07 执行后逐项打勾，补充命令输出和截图路径。 | 所有 MVP 项通过或有明确回退说明。 |
| `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md:26` | 实测各路电源电压与空载/负载电流 | 用万用表测量 OPi5、i.MX6ULL、舵机电源、负载电源的电压；记录空载与典型工作电流。 | 测试记录包含电压、电流、负载状态、是否掉压。 |
| `docs/03` | 补 3.7 供电预算 P0 行 | 万用表测 5V（MQ-2 加热 + 蜂鸣器）/3.3V（GPIO 上拉、RGB、PIR/火焰）两路电流。 | 表中写入实测电压/电流。 |
| `docs/06` | 标定 P0 误报与阈值 | 记录门开关、人体触发、打火机近火焰、烟雾近 MQ-2 四类场景的 sensors 与 risk_score。 | 至少记录 NORMAL/VERIFY/ALARM 三类场景。 |
| `docs/04_软件架构与模块划分_Linux边缘控制版.md:40` | 实测 i.MX6ULL 控制循环抖动和端到端延迟 | 在 Task07 中记录采样、抓拍、AI 请求、上报的时间戳，计算 min/avg/max。 | 测试记录包含至少 20 次循环/事件延迟统计。 |
| `docs/06_状态机与联动机制_Linux实现版.md:52` | 根据实测误报率调整 risk_score 阈值 | 联调阶段记录不同场景 risk_score 与误报，调整加分表。 | 至少记录 NORMAL/WARN/ALARM 三类场景，并说明阈值调整。 |
| `docs/07_端边HTTP_JSON_Contract.md:149` | 标定 HTTP 超时与端到端延迟 | Task07 中记录 capture/ai/post 总耗时，按结果调整超时。 | 至少 20 次请求统计，并给出最终超时参数。 |
| `docs/08_iMX6ULL视觉采集_云台_Web服务器方案.md:55` | 实测 MG90 角度、稳定时间和 PWM 脉宽 | 先空载测 PWM，再接单个 MG90，记录 60/90/120 度对应脉宽和稳定时间。 | 云台无明显抖动，抓拍时画面稳定。 |
| `docs/10_设备健康监测与扩展传感器方案.md:40` | 标定设备健康阈值 | 分别采集静止、水泵/云台动作两类数据。 | 每类至少一段样本，给出 RMS/峰峰值均值。 |
| `docs/12_材料清单与获取计划_新硬件版.md:44` | 补充材料实测供电能力 | 测量空载/负载电压，记录舵机动作和 OPi5 推理时是否掉压。 | 材料表中写入实测电压、电流和是否可用于最终演示。 |
| `docs/13_实训报告与答辩演示方案_迁移版.md:68` | 补充最终截图和录屏路径 | 在最终联调时保存截图到 `assets/photos/` 或 `project/report/images/`，录屏放本地不入库或压缩后提交。 | 每张截图有说明，PPT 可直接引用。 |
