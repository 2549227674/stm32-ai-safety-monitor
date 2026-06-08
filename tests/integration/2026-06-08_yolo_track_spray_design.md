# 2026-06-08 YOLO 火源检测 + 低速视觉锁定 + 双确认喷水 + 低帧视觉流 设计评审

> 拟落库路径：`tests/integration/2026-06-08_yolo_track_spray_design.md`
> 性质：**设计评审文档（review gate）**。本轮只评审与定边界，不写执行代码、不改 `imx_safetyd.c` 主闭环。通过评审后才出 `CLAUDE_CODE_TASK` 执行 prompt。
> 分支：`migration/imx6ull-opi5-edge-ai`
> 关联：`CANONICAL_DECISIONS.md`、`docs/06`、`docs/07`、`docs/08`、`docs/09`、`docs/11`、`docs/13/14`

---

## 0. 一句话目标

云台同时挂载**摄像头 + 水枪发射器**：YOLO 在 OPi5 检测火源并给出目标位置 → i.MX6ULL 本地限幅驱动云台**低速锁定** → 在**本地传感器与视觉双确认**下，由 i.MX 本地状态机经 PCA9685 CH6 喷水灭火；Dashboard 以**低帧 MJPEG 标注流**展示锁定过程。

口径锁定：称之为「**低速视觉锁定 + 本地双确认处置**」，**不称"实时追踪/实时监控"**（见 §8 答辩）。

---

## 1. 两条不可违反的铁律（整个设计围绕它们）

1. **`imx_safetyd` 主闭环不参与追踪。** `flame=1 或 mq2=1 → ALARM → 蜂鸣器/RGB/继电器` 保持紧凑、本地、独立，永不被追踪逻辑或网络阻塞。追踪进程崩溃，安全报警照常。
2. **喷水命令永远由 i.MX 本地 FSM 发出，`control_allowed=false`。** YOLO/OPi5 的输出对喷水**永远不是充分条件**，只作"瞄准 + 置信度增强"输入。

这两条与 `CANONICAL 0.8` 硬约束一致，本设计不松动。

---

## 2. 当前状态盘点（评审基线）

| 项 | 现状 | 证据 |
|---|---|---|
| P0 本地闭环 | 已完成（flame/mq2/pir 真实读，buzzer/RGB 真驱动，强触发本地直接 ALARM） | Task10-B/C/D/E、`tests/imx6ull/2026-06-07_p0_*.md` |
| P1 执行器 | 已完成（继电器 CH5、水泵+水枪 CH6、OLED、SoC 温度） | Task11-A/B/C/D |
| 云台 | 三点扫描（pan 60/90/120，tilt 90，800ms），`pca9685_set_pose.c` 强制 1100–1900us、先 tilt 后 pan | docs/08 §8.4、Task08-A |
| OPi5 AI | mock + qwen3vl 两后端；Qwen3-VL `objects=[]`（VLM 无 bbox）；worker ~8.4s | docs/09、`tests/opi5/2026-06-07_qwen3vl_*.md` |
| 契约 | docs/07：`objects[].bbox=[x1,y1,x2,y2]`、`risk_hint 0–10`、`control_allowed=false`，字段只可向后兼容新增 | docs/07 §7.3/7.8/7.9 |
| Dashboard | 事件/快照驱动；已有 `#visionImage`/`#objectsList`/`#crosshair`，但 objects 仅**文本**展示，无画框、无视频流 | `server/backend/static/dashboard.js` `renderAIVision/renderObjects/renderImage` |

### 2.1 关键纠正：`yolov8n_phase10` 不是火焰模型

OPi5 上 `phase10/models/yolov8n_phase10/` 的真实权重为：

```
yolov8n_metal_nut_cable_best.onnx
yolov8n_metal_nut_cable_best_airockchip_fp16.rknn
yolov8n_metal_nut_cable_best_fp16.rknn
```

即**金属螺母/线缆**工业缺陷检测微调模型，**不认 fire/smoke/person**。结论：

- **能用**：证明 YOLOv8n→RKNN→RK3588 NPU 管线已跑通（配套 `production_yolo_soak_runner.py`、`smoke_test_rknn.py`、`compare_onnx_rknn.py`、`diag_rknn.py`，以及 `rknn-toolkit2` 的 `rknn_yolov5_demo`）。
- **不能用**：类别错，不能直接"锁火源"。

### 2.2 权重来源决策（已定：A 方案，真·火源检测）

**已拍板：走 A（fire/smoke 微调权重），不走 person 兜底。**

- **首选权重**：`luminous0219/fire-and-smoke-detection-yolov8`（GitHub）—— 架构是 **YOLOv8n**、类别 `fire`/`smoke`、提供可下载 `.pt`，与板上已验证的 `yolov8n_phase10` 架构**完全一致**，转 RKNN 风险最低。
- **备选来源**：Roboflow Universe 上带 `yolov8n` 卡片的火焰/烟雾项目（可直接下预训练权重；Middle East Tech University 的 `fire-and-smoke-detection-hiwia` 为 **MIT 许可**，最干净）。
- **person 兜底**：本轮不采用；仅在 A 在板上反复失败时才回退，火焰列为扩展类别（docs/09 §9.3 允许）。

> **[CLAUDE_CODE_TODO | VERIFY]** 火源权重转换链与板端可运行性（关键路径）
> - 为何 GPT 给不了：沙箱无法访问 OPi5 NPU，转换/精度只能板端实测。
> - 期望产物/操作：
>   1. 取 luminous0219 yolov8n `.pt`（首选；不入库）。
>   2. **走 phase10 同一条 airockchip 链**：`airockchip/ultralytics_yolov8` 分支导出 ONNX（改过检测头，RKNN 能吐干净 bbox）→ `rknn-toolkit2` 转 `.rknn`。**勿用官方 ultralytics 直接 export 给 RKNN**（检测头/NMS 对不上，是最常见踩坑）。
>   3. OPi5 上单帧推理，确认输出合理 `fire`/`smoke` bbox。
> - 回填位置：`tests/opi5/YYYY-MM-DD_yolo_fire_rknn.md`；`edge/opi5-ai/models/README.md`；docs/09 §9.3。
> - 验收：① 类别名清单；② 转换命令全程；③ 真实 bbox 截图；④ 单帧 NPU 推理 ms；⑤ 选定 conf 阈值（初值 fire≈0.5）；⑥ 权重/数据集**未入库**。
> - 失败回退：若转换或精度反复不达标，回退 person 兜底并在报告注明火焰为扩展类别。

---

## 3. 双确认喷水逻辑（核心，落 docs/06 状态机）

**不新增顶层状态，保持已验证的 5 态 FSM（NORMAL/WARN/VERIFY/ALARM/FAULT）不被推翻。** 追踪与喷水作为 **ALARM 内的子行为**实现（`alarm_phase`：`AIMING` / `SUPPRESSING`，仅经 `raw_json` 透出，不改 DB schema）。答辩口径：未动安全 FSM，只在 ALARM 内加了**本地传感器闸门**的处置子行为。

### 3.1 喷水真值表

| 本地 flame/mq2 | 视觉确认 fire（窗口内 score≥阈值） | 行为 |
|:---:|:---:|---|
| 1 | 1 | ALARM → 云台锁定目标 → **精确喷水**（aimed）。★ 高光 demo |
| 1 | 0 / 视觉不可用 / 超时 | ALARM 照常（蜂鸣器/RGB/继电器）→ **降级**：见 §3.3 |
| 0 | 1 | **不喷**；仅 `WARN` + 记录"视觉疑似火情"（视觉单独不足，防误喷） |
| 0 | 0 | NORMAL |

要点：**本地传感器是喷水的必要条件（永远）；视觉是瞄准 + 置信度增强，永不单独触发喷水。** `control_allowed` 始终 `false`，喷水命令由 i.MX FSM 发出。

### 3.2 喷水安全约束（新增，落 docs/06 + CANONICAL）

- **最大喷水时长** `SPRAY_MAX_MS`（建议初值 3000ms）→ 到时强制停泵，防连续喷。
- **冷却** `SPRAY_COOLDOWN_MS`（建议 2000ms）→ 冷却期内不再触发喷水。
- 退出/复位 `all_off()` 保证 `relay=0, pump=0`（沿用 P1 已验证行为）。
- CH6 仍 active high、默认 OFF、双负载（水泵+水枪），隔离水箱、水电物理隔离、不接 220V（CANONICAL 0.7/0.8 不变）。

### 3.3 视觉掉线降级（待决项，本文已落推荐值）

> **[CLAUDE_CODE_TODO | DECIDE]** 本地=1 但视觉不可用/超时时是否默认位自动喷
> - 推荐（本文默认）：`VISION_OFFLINE_AUTOSPRAY=1` —— 本地触发后等待 `T_confirm`（建议 1500ms）仍无视觉确认，则**云台保持默认/当前位、自动喷水**。理由：保住"AI/网络掉线仍能本地灭火"的独立性叙事（最强）。
> - 备选（更保守）：`VISION_OFFLINE_AUTOSPRAY=0` —— 仅 ALARM 报警，喷水等人工确认。适合对误喷极敏感的场景。
> - 为何 GPT 给不了：取决于演示现场对误喷的容忍度，需你拍板。
> - 回填位置：docs/06；CANONICAL；`imx-safetyd.env`。
> - 验收：env 开关可切；两种行为各有一次实测记录。

无论哪种，**核心安全响应（蜂鸣器/RGB/继电器）都不依赖视觉**，本地闭环独立性成立。

---

## 4. 架构与三机分工（完整追踪流版）

角色不变，重活全压给强的 RK3588，弱的 i.MX 只做透传 + 执行：

| 节点 | 职责 |
|---|---|
| **i.MX6ULL** | ① `imx_safetyd`（不变 + 加喷水闸）：安全 FSM、**独占 PCA9685 全部通道**、喷水决策、读视觉确认标志。② **新进程 `imx_tracker`**：摄像头**原生 MJPEG** 连续抓帧 → POST OPi5 → 收 `target_center` → 计算限幅步进 → 经**本地 IPC** 请求 `imx_safetyd` 调云台（CH0/CH1）。**tracker 只提议瞄准，safetyd 仲裁，安全优先。** |
| **Orange Pi 5** | `yolo_rknn` 后端：每帧 YOLO → 画框 → ① 回 `target_center/offset/objects/best_fire` 给 i.MX；② 标注帧入环形缓冲，经 `/api/track/stream` 出**低帧 MJPEG**。一条流水线，两个消费者。 |
| **Flask/Dashboard** | 嵌 OPi5 标注流 + 锁定指示 + `alarm_phase` + 喷水倒计时；事件/状态沿用。 |

### 4.1 为什么这么分

- i.MX6ULL（Cortex-A7）扛不住连续 YOLO+标注+出流。**强制摄像头原生 MJPEG，绝不 YUYV→JPEG 软编码。** i.MX 只"抓帧透传 + 收坐标 + 转舵机"。
- **单一总线属主**：`imx_safetyd` 独占 `/dev/i2c-0 0x40`；`imx_tracker` 经 Unix socket/FIFO 发 `set_gimbal_pose pan_us tilt_us`，safetyd 在非更高优先级安全动作时才执行并二次限幅。tracker 崩溃不影响安全回路与总线。
- **视觉确认标志传递**：`imx_tracker` 写小状态文件（如 `/run/imx_visual_state.json`：`{fire_confirmed, score, ts}`），`imx_safetyd` 每个 FSM tick 读取，作为 §3.1 的视觉确认输入。

### 4.2 进程边界图（拟入 docs/04）

```text
[USB Cam]→(MJPEG)→ imx_tracker ──POST /api/track/frame──▶ OPi5 yolo_rknn
                       │                                      │
                       │◀──── target_center/offset ───────────┘
                       │ write /run/imx_visual_state.json (fire_confirmed)
                       │ IPC: set_gimbal_pose(pan_us,tilt_us)
                       ▼
                  imx_safetyd ──(PCA9685 CH0/CH1 限幅)──▶ 云台
                       │  读 flame/mq2(本地) + 读 visual_state
                       │  双确认闸 → CH6 喷水(限时+冷却)
                       ▼
                  /api/events → Flask → Dashboard(嵌 OPi5 /api/track/stream)
```

---

## 5. 契约扩展（docs/07，向后兼容，不改 DB schema）

新增两个端点 + 一个字段，均为向后兼容新增（§7.9）；旧后端经 `raw_json` 忽略。

### 5.1 `POST /api/track/frame`（低延迟，追踪环用）

请求：`multipart/form-data`，`image`(JPEG) + `metadata`(JSON，含 `frame_id`、`resolution`)。
响应（示例，**纯感知，非命令**）：

```json
{
  "ok": true,
  "contract_version": "1.1",
  "frame_id": 1234,
  "latency_ms": 41,
  "objects": [{"label": "fire", "score": 0.82, "bbox": [300,120,460,300]}],
  "tracking_hint": {
    "target_label": "fire",
    "target_center": [380, 210],
    "frame_center": [320, 240],
    "offset_px": [60, -30],
    "confidence": 0.82
  },
  "best_fire": {"score": 0.82, "bbox": [300,120,460,300]},
  "control_allowed": false
}
```

- `tracking_hint` 是**提示**，不含 `pan_delta_deg`/`tilt_delta_deg`（角度增量由 i.MX 本地计算+限幅，避免 OPi5 像在"下发动作"）。
- `objects[].bbox` 此后由 yolo 后端**真正填充**（VLM 路径仍 `objects=[]`，不伪造）。

### 5.2 `GET /api/track/stream`（低帧标注流，Dashboard 用）

`multipart/x-mixed-replace; boundary=frame`，标注后的 MJPEG，建议 ≤5fps、640×480。**仅展示用，不参与控制。**

### 5.3 `/api/infer/vision`（事件快照，保持）

mock/qwen3vl/**新增 yolo_rknn** 三后端；事件路径不变。

> **[CLAUDE_CODE_TODO | DECIDE]** `contract_version` 是否从 1.0 升 1.1
> - 期望产物/操作：新增字段/端点后小版本号 +0.1，docs/07 §7.9 记录；旧字段不删。
> - 回填位置：docs/07。
> - 验收：mock/qwen3vl 旧响应在新版本下仍合法。

---

## 6. OPi5 后端（`yolo_rknn`，mock 优先）

- 新增 `AI_BACKEND=yolo_rknn`，复用 phase10 已验证的 RKNN 推理管线，换 §2.1 选定的 fire/person 权重。
- **risk_hint 映射新写**：YOLO 无文本，按 label→risk（`person`=低、`fire`/`smoke`=高），**仍守 0–10 量纲**（勿引入 0–100，那是已解决的旧冲突）。
- **不要让 hybrid 成默认**：YOLO 单帧几十 ms，Qwen3-VL ~8s；hybrid≈VLM 延迟。追踪/锁定用 **yolo 单后端**；hybrid 仅留给"对被标记帧做一句解释"的事件路径。
- mock 优先：先让后端在无真实权重时返回 mock bbox/`tracking_hint`，把整链 + Dashboard 跑通，再接真权重。
- 权重/抓拍/数据集**不入库**（docs/09 §9.9、CANONICAL 0.8）。

---

## 7. 延迟预算（拟入 docs/11）

| 环节 | 估计 | 说明 |
|---|---|---|
| YOLOv8n @ RK3588 NPU | ~10–30ms | 真正快的部分 |
| i.MX 抓帧(MJPEG 原生) | ~30–80ms | 不软编码 |
| 过网 POST + 回包(LAN, ~10–20KB) | ~50–150ms | 两子网，PC 静态路由 |
| 云台步进 + MG90 settle | ~100–300ms | 单次小步 |
| **追踪环整体** | **~3–5Hz** | 低速锁定，落在 SMP PREEMPT 100–500ms 软实时区间 |

> **[CLAUDE_CODE_TODO | MEASURE]** 标定追踪环各段实测延迟与稳定帧率
> - 期望产物/操作：≥30 次循环统计抓帧/推理/过网/转舵机耗时与有效 Hz。
> - 回填位置：docs/07 §7.7、docs/11、本文 §7。
> - 验收：给出实测帧率与各段 ms。

省 i.MX：**事件门控出流** —— 空闲单张快照，进 VERIFY/ALARM 才把流拉到 5fps。

---

## 8. 答辩防雷（叙事变化，落 docs/13/14）

- **绝不说"实时追踪/实时监控"**；说"**低速视觉锁定 + 本地双确认处置**"。占住"安全优先于速度"高地。
- 新增问答：
  - "AI 会不会乱喷水？" → 不会。喷水必要条件是**本地火焰/烟雾传感器**；AI 单独不足；`control_allowed=false`；且限时+冷却+隔离水箱。
  - "为什么不是实时？" → 人机尺度软实时（3–5Hz），watchdog + 默认安全态 + 离线降级；速度不是安全前提。
  - "这还算数字系统吗？" → 数字系统证据是 PCA9685 PWM 云台、MOS 驱动水泵、I2C、GPIO 与逻辑分析仪波形；YOLO 只是感知源。把这些摆最前。
- 创新点新增一条：「OPi5 目标检测 bbox + i.MX 本地限幅云台辅助指向 + 物理/视觉双确认主动处置」。

---

## 9. 文档影响清单（本轮是"改写"不是"补丁"）

| 文档 | 改动 | 必要性 |
|---|---|---|
| **docs/06** 状态机 | ALARM 内加 `AIMING/SUPPRESSING` 子行为 + 双确认闸 + 喷水限时/冷却 + 降级开关 | 必须 |
| **docs/07** 契约 | 新增 `/api/track/frame`、`/api/track/stream`、`tracking_hint`；`objects[].bbox` 真填充；版本 1.1 | 必须 |
| **CANONICAL** | 登记 `imx_tracker` 进程、PCA9685 属主、双确认闸、`SPRAY_MAX_MS`/冷却、`VISION_OFFLINE_AUTOSPRAY`，重申 `control_allowed=false` | 必须（唯一事实源） |
| **docs/04** 软件架构 | 新增 `imx_tracker` + IPC + OPi5 流端点，模块图重画 | 高 |
| **docs/08** 视觉/云台/web | 抓拍改连续 MJPEG；云台加"低速锁定"变体；Dashboard 加叠框 + 流 | 高 |
| **docs/09** OPi5 AI | 新增 yolo_rknn 后端 + 标注流 + §2.1 权重纠正；三后端并存 | 高 |
| **docs/02/05** 需求/验收 | 补主动处置、追踪、流、双确认、降级、防误喷用例 | 中 |
| **docs/11** 测试矩阵 | 加追踪/流/双确认/降级/误喷/喷时长/延迟用例 | 中 |
| **docs/13/14** 报告答辩 | 重写 AI/云台/创新点 + 新增问答 | 收尾 |
| **docs/12** BOM | 水枪已有，标注"纯软件/无新硬件" | 低 |

不动：DB schema、引脚/通道分配、安全红线、不开 doc 15+。

---

## 10. 执行计划（mock 优先垂直切片，每步可停可演）

执行 prompt 落于 `CLAUDE_CODE_TASK_12_yolo_fire_track_spray.md`（沿用 Task10/Task11 的"一任务多子切片"体例）。一片不过不进下一片。

| 切片 | 内容 | 触不触 safetyd | 状态 | 可演产物 |
|---|---|---|---|---|
| 0 | **本评审** | 否 | gate | 评审通过（§14） |
| 12-A | OPi5 `yolo_rknn` mock 后端 + `/api/track/frame` + `/api/track/stream`（mock 标注 MJPEG） | 否 | **可立刻开工** | OPi5 端假数据流通 |
| 12-B | Dashboard：静帧/流叠真实 bbox + 锁定标记 + `alarm_phase` + 嵌低帧流 | 否 | **可立刻开工**（依赖 12-A 契约） | 整链假数据跑通，无喷水 |
| 12-C | 接真火源权重（§2.2 luminous0219 yolov8n，airockchip→rknn） | 否 | **VERIFY 门控** | 真 bbox + 真标注流 |
| 12-D | `imx_tracker`：抓帧转发 + 经 IPC 调云台，**只瞄准、不喷水** | 仅加 IPC 入口 | 依赖 12-A/12-C | 低速锁定 demo |
| 12-E | `imx_safetyd` 喷水闸（双确认 + 限时 + 冷却 + 降级） | **是，最后做** | **评审门控** | 双确认精确喷水 |
| 12-F | 调优 | — | 收尾 | 帧率/阈值/延迟/喷时长实测回填 |

改 `imx_safetyd.c` 喷水闸（12-E）**必须先经本评审通过**（你的硬规则）。

---

## 11. 风险与回退

| 风险 | 缓解 / 回退 |
|---|---|
| i.MX 连续抓帧/转发负载过高 | 原生 MJPEG only；事件门控出流；降帧/降分辨率；回退到三点扫描 |
| 误喷 | 双确认（本地必要）+ 隔离水箱 + 限时 + 冷却 |
| tracker 崩溃 | 与 safetyd 分进程；safetyd 独立 ALARM + 降级喷水 |
| I2C 总线争用 | safetyd 单一属主 + IPC，安全动作优先 |
| 火源权重不可得 | person 兜底打通，火焰列为扩展类别（docs/09 §9.3 允许） |
| 范围失控 | 每切片独立可演，可在任一步停下 |

---

## 12. 待决项汇总（请拍板）

1. `VISION_OFFLINE_AUTOSPRAY`：本文默认 **1（默认位自动喷，保独立性）**；要更保守改 0。
2. ~~YOLO 权重选 fire 还是 person~~ **已定：A 方案，首选 luminous0219 yolov8n（§2.2），person 仅作失败回退。**
3. `contract_version` 是否升 1.1（§5.3 DECIDE）。
4. 阈值/延迟/喷时长实测回填（§3.2、§7 MEASURE）。

---

## 13. 验收标准（终态）

- [ ] yolo_rknn 后端出真实 fire/person bbox；risk_hint 0–10；`control_allowed=false`。
- [ ] `/api/track/frame` 返回 `tracking_hint`（纯感知）；`/api/track/stream` 出 ≤5fps 标注流。
- [ ] Dashboard：抓拍/流上叠真实 bbox + 锁定标记（与云台姿态标记区分）+ `alarm_phase`。
- [ ] `imx_tracker` 经 IPC 驱动云台低速锁定；单步 ≤10° pan / ≤5° tilt；脉宽 1100–1900us 不破。
- [ ] 双确认喷水：本地+视觉同时成立才精确喷；视觉单独成立**不喷**；限时 + 冷却生效。
- [ ] **独立性**：停 OPi5/停 tracker/拔网，制造 flame/mq2 强触发，本地仍 ALARM + 蜂鸣器/RGB；按 §3.3 降级喷水或人工确认。
- [ ] 全程 `control_allowed=false`；AI/OPi5/Flask 未直接驱动任何执行器。
- [ ] 权重/抓拍/数据集未入库。

---

## 14. 评审决议（待填）

- [ ] 评审通过，可出 Task05-D/05-E、Task08-B、Task08-C 执行 prompt。
- [ ] 待决项 §12 已拍板：__________
