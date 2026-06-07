const STATE_META = {
  NORMAL: {
    color: "var(--normal)",
    label: "正常巡检",
    lamp: "GREEN",
    desc: "本地传感器无异常，AI 无风险提示。执行器 OFF，RGB 绿灯常亮。",
  },
  WARN: {
    color: "var(--warn)",
    label: "轻微异常",
    lamp: "AMBER",
    desc: "设备健康或低风险输入告警。记录事件，不升级火灾 ALARM。",
  },
  VERIFY: {
    color: "var(--verify)",
    label: "复核中",
    lamp: "BLUE",
    desc: "本地传感器触发，请求 OPi5 Qwen3-VL 视觉复核。执行器保持安全态，RGB 蓝灯。",
  },
  ALARM: {
    color: "var(--alarm)",
    label: "报警",
    lamp: "RED",
    desc: "火焰 / 烟雾本地强触发，i.MX6ULL 直接进入 ALARM。蜂鸣器、RGB、继电器与水泵按规则动作。",
  },
  FAULT: {
    color: "var(--fault)",
    label: "故障 / 降级",
    lamp: "VIOLET",
    desc: "摄像头、AI 或网络异常。本地降级继续运行，记录故障，不崩溃。",
  },
  UNKNOWN: {
    color: "var(--offline)",
    label: "等待数据",
    lamp: "UNKNOWN",
    desc: "暂无事件或旧事件字段不足。Dashboard 保持兼容显示。",
  },
};

const SCENARIO_CHIPS = [
  ["NORMAL", "NORMAL"],
  ["PIR · VERIFY", "VERIFY"],
  ["FLAME · ALARM", "ALARM"],
  ["MQ-2 · ALARM", "ALARM"],
  ["OPi5 OFFLINE", "FAULT"],
  ["SoC TEMP · WARN", "WARN"],
  ["FLASK SPOOL", "FAULT"],
];

const els = {
  frame: document.getElementById("frame"),
  clockTime: document.getElementById("clockTime"),
  refreshText: document.getElementById("refreshText"),
  imxLink: document.getElementById("imxLink"),
  opi5Dot: document.getElementById("opi5Dot"),
  opi5Link: document.getElementById("opi5Link"),
  flaskDot: document.getElementById("flaskDot"),
  flaskLink: document.getElementById("flaskLink"),
  stateHero: document.getElementById("stateHero"),
  currentState: document.getElementById("currentState"),
  statePulse: document.getElementById("statePulse"),
  stateDesc: document.getElementById("stateDesc"),
  riskScore: document.getElementById("riskScore"),
  riskBar: document.getElementById("riskBar"),
  deviceId: document.getElementById("deviceId"),
  seq: document.getElementById("seq"),
  needSnap: document.getElementById("needSnap"),
  rgbLamp: document.getElementById("rgbLamp"),
  timestamp: document.getElementById("timestamp"),
  contractVersion: document.getElementById("contractVersion"),
  controlAllowedBadge: document.getElementById("controlAllowedBadge"),
  sensorGrid: document.getElementById("sensorGrid"),
  actuatorGrid: document.getElementById("actuatorGrid"),
  aiState: document.getElementById("aiState"),
  imageEmpty: document.getElementById("imageEmpty"),
  visionImage: document.getElementById("visionImage"),
  crosshair: document.getElementById("crosshair"),
  frameId: document.getElementById("frameId"),
  panTilt: document.getElementById("panTilt"),
  imageLink: document.getElementById("imageLink"),
  aiOffline: document.getElementById("aiOffline"),
  aiOfflineText: document.getElementById("aiOfflineText"),
  vlmModel: document.getElementById("vlmModel"),
  riskHint: document.getElementById("riskHint"),
  aiOk: document.getElementById("aiOk"),
  faceMetric: document.getElementById("faceMetric"),
  aiSummary: document.getElementById("aiSummary"),
  aiExplanation: document.getElementById("aiExplanation"),
  labelsList: document.getElementById("labelsList"),
  objectsList: document.getElementById("objectsList"),
  facesList: document.getElementById("facesList"),
  latencyStrip: document.getElementById("latencyStrip"),
  controlAllowed: document.getElementById("controlAllowed"),
  spoolBadge: document.getElementById("spoolBadge"),
  eventCount: document.getElementById("eventCount"),
  errorBox: document.getElementById("errorBox"),
  eventsBody: document.getElementById("eventsBody"),
  degradeMode: document.getElementById("degradeMode"),
  degradeList: document.getElementById("degradeList"),
  scenarioChips: document.getElementById("scenarioChips"),
};

function fitStage() {
  const scale = Math.min(window.innerWidth / 1920, window.innerHeight / 1080);
  els.frame.style.transform = `scale(${scale})`;
}

function normalizeEvent(event) {
  if (!event || typeof event !== "object") return null;

  const sensors = objectOrEmpty(event.sensors);
  const actuators = objectOrEmpty(event.actuators);
  const vision = objectOrEmpty(event.vision);
  const ai = objectOrNull(event.ai_result);
  const vlm = objectOrEmpty(ai?.vlm_result);
  const panTilt = objectOrEmpty(vision.pan_tilt || event.pan_tilt);
  const imageUrl = event.image_url || vision.image_url || "";

  return {
    ...event,
    state: String(event.state || "UNKNOWN").toUpperCase(),
    risk_score: clamp(Number(event.risk_score ?? 0), 0, 10),
    sensors,
    actuators,
    vision: {
      ...vision,
      image_url: imageUrl,
      pan_tilt: {
        pan_deg: panTilt.pan_deg,
        tilt_deg: panTilt.tilt_deg,
      },
    },
    ai_result: ai
      ? {
          ...ai,
          objects: Array.isArray(ai.objects) ? ai.objects : [],
          faces: Array.isArray(ai.faces) ? ai.faces : [],
          vlm_result: {
            ...vlm,
            labels: Array.isArray(vlm.labels) ? vlm.labels : [],
          },
        }
      : null,
    image_url: imageUrl,
    latency_ms: event.latency_ms || ai?.latency_ms || vlm.timing || null,
    contract_version: event.contract_version || ai?.contract_version || "1.0",
    device_health: event.device_health || "UNKNOWN",
  };
}

function renderDashboard(latest, events) {
  renderTopBar(latest);
  renderStateHero(latest);
  renderSafetyLock(latest);
  renderSensorsPanel(latest);
  renderActuatorsPanel(latest);
  renderAIVision(latest);
  renderEventStream(events);
  renderDegradeEvidence(latest);
  renderBottomBar(latest);
}

function renderTopBar(event) {
  const device = event?.device_id || "labbox_001";
  const contract = event?.contract_version || "1.0";
  const ai = event?.ai_result;
  const aiOffline = isAiOffline(event);
  const flaskSpool = isFlaskSpooling(event);

  els.imxLink.textContent = event ? "SAFETY-FSM" : "WAITING";
  els.opi5Link.textContent = aiOffline ? "OFFLINE" : ai ? "RKNN·UP" : "IDLE";
  els.flaskLink.textContent = flaskSpool ? "SPOOL" : "UP";
  setDot(els.opi5Dot, aiOffline ? "down" : ai ? "up" : "idle");
  setDot(els.flaskDot, flaskSpool ? "warn" : "up");
  els.refreshText.textContent = `${device} · contract ${contract} · ${new Date().toLocaleTimeString()}`;
}

function renderStateHero(event) {
  const state = event?.state || "UNKNOWN";
  const meta = STATE_META[state] || STATE_META.UNKNOWN;
  const risk = event ? event.risk_score : 0;
  const riskColor = risk >= 6 ? "var(--alarm)" : risk >= 3 ? "var(--warn)" : "var(--normal)";

  els.stateHero.className = `statehero ${stateClass(state)}`;
  els.stateHero.style.borderColor = `color-mix(in srgb, ${meta.color} 34%, transparent)`;
  els.stateHero.style.background = `linear-gradient(180deg, color-mix(in srgb, ${meta.color} 9%, var(--panel)) 0%, var(--night) 130%)`;
  els.stateHero.querySelector(".glow").style.background =
    `radial-gradient(440px 200px at 88% -10%, color-mix(in srgb, ${meta.color} 22%, transparent), transparent 70%)`;
  els.currentState.textContent = state;
  els.currentState.style.color = meta.color;
  els.statePulse.style.color = meta.color;
  els.statePulse.style.background = meta.color;
  els.statePulse.style.boxShadow = `0 0 16px ${meta.color}`;
  els.stateDesc.textContent = meta.desc;
  els.riskScore.textContent = event ? String(risk) : "-";
  els.riskScore.style.color = event ? riskColor : "var(--faint)";
  els.deviceId.textContent = event?.device_id || "-";
  els.seq.textContent = event?.seq ?? "-";
  els.needSnap.textContent = event ? String(Boolean(event.need_snap)) : "-";
  els.needSnap.style.color = event?.need_snap ? "var(--lime)" : "var(--faint)";
  els.rgbLamp.textContent = meta.lamp;
  els.rgbLamp.style.color = meta.color;
  els.timestamp.textContent = event?.timestamp ? shortTime(event.timestamp) : "-";
  els.contractVersion.textContent = event?.contract_version || "-";
  els.riskBar.innerHTML = Array.from({ length: 10 }, (_, index) => {
    const filled = event && index < risk;
    return `<div class="risk-seg" style="${filled ? `background:${riskColor};box-shadow:0 0 8px ${riskColor}` : ""}"></div>`;
  }).join("");
}

function renderSafetyLock(event) {
  const allowed = event?.ai_result?.control_allowed === true;
  els.controlAllowed.textContent = String(allowed);
  els.controlAllowedBadge.textContent = allowed ? "control_allowed = true" : "control_allowed = false";
  els.controlAllowedBadge.className = allowed ? "badge hot" : "badge violet";
}

function renderSensorsPanel(event) {
  const sensors = event?.sensors || {};
  const health = event?.device_health || "UNKNOWN";
  const temp = sensors.soc_temp;
  const tempText = temp === undefined || temp === null ? "—" : `${Number(temp).toFixed(1)}`;
  const tempWarn = health === "WARN";
  const items = [
    sensorTile("PIR", "gpio117 · D0", sensors.pir, "/1", "人体红外 · raw 0=空闲 1=触发", Number(sensors.pir) ? "hot" : "ok"),
    sensorTile("FLAME", "gpio119 · D2", sensors.flame, "/1", "火焰 DO · 3.3V · 高有效", Number(sensors.flame) ? "hot" : "ok"),
    sensorTile("MQ-2", "gpio120 · D3", sensors.mq2, "/1", "烟雾数字量 · 课设短时 DO 触发（非浓度）", Number(sensors.mq2) ? "hot" : "ok"),
    sensorTile("DOOR", "gpio118 · D1", "—", "", "门磁 · skipped/disabled，待补上拉后重测", "skip", true),
  ];
  els.sensorGrid.innerHTML = items.join("") + `
    <div class="tile ${tempWarn ? "warnstate" : ""}" style="grid-column:1 / -1">
      <div class="th"><span class="tname">SoC_TEMP · 设备热健康</span><span class="tpin">thermal_zone0</span></div>
      <div class="tbody">
        <div class="tval">${escapeHtml(tempText)}<span class="tunit">°C</span></div>
        <span class="badge ${tempWarn ? "warn" : health === "NORMAL" ? "ok" : "skip"}">device_health=${escapeHtml(health)}</span>
      </div>
      <div class="tdesc">主控芯片温度 · 非环境温度 · 不升级火灾 ALARM</div>
    </div>`;
}

function renderActuatorsPanel(event) {
  const a = event?.actuators || {};
  const state = event?.state || "UNKNOWN";
  const rgb = rgbColor(a, state);
  els.actuatorGrid.innerHTML = `
    ${actuatorTile("BUZZER", "gpio122", a.buzzer, "active-low + NPN 三极管 · 不经 PCA9685", Number(a.buzzer) ? "SOUNDING" : "SILENT")}
    <div class="tile">
      <div class="th"><span class="tname">RGB LAMP</span><span class="tpin">PCA9685 CH2-4</span></div>
      <div class="tbody">
        <div class="rgb-lamp" style="background:${rgb};box-shadow:0 0 18px ${rgb}, inset 0 0 8px rgba(255,255,255,0.3)"></div>
        <div style="text-align:right"><div class="tdesc mono">r:${a.rgb_r ?? 0} g:${a.rgb_g ?? 0} b:${a.rgb_b ?? 0}</div><span class="badge lime">视觉增强</span></div>
      </div>
      <div class="tdesc">状态灯 · 绿/蓝/红/紫随 FSM</div>
    </div>
    <div class="tile ${Number(a.relay) ? "active" : ""}">
      <div class="th"><span class="tname">RELAY</span><span class="tpin">PCA9685 CH5</span></div>
      <div class="tbody"><div class="relay-toggle ${Number(a.relay) ? "on" : ""}"><div class="knob"></div></div><span class="badge ${Number(a.relay) ? "hot" : "ok"}">${Number(a.relay) ? "动作" : "默认 OFF"}</span></div>
      <div class="tdesc">KY-019 5V · active-high · ALARM=1</div>
    </div>
    ${actuatorTile("PUMP / 水枪", "PCA9685 CH6", a.pump, "CH6→MOS→水泵+水枪双负载 · 隔离水箱", Number(a.pump) ? "喷淋" : "默认 OFF")}
  `;
}

function renderAIVision(event) {
  const ai = event?.ai_result;
  const vision = event?.vision || {};
  const panTilt = vision.pan_tilt || {};
  const imageUrl = event?.image_url || vision.image_url || "";
  const labels = ai?.vlm_result?.labels || [];
  const explanation = ai?.explanation || ai?.vlm_result?.explanation || ai?.vlm_result?.description || "-";
  const model = ai?.vlm_result?.model || ai?.model || "qwen3-vl-2b";
  const objects = ai?.objects || [];
  const faces = ai?.faces || [];

  els.aiState.textContent = ai ? "解释增强 · 非控制源" : "待机 · 未请求推理";
  els.frameId.textContent = vision.frame_id ? `V4L2 · frame ${vision.frame_id}` : "无快照 · need_snap=false";
  els.panTilt.textContent = `pan ${panTilt.pan_deg ?? "-"}° · tilt ${panTilt.tilt_deg ?? "-"}°`;
  els.vlmModel.textContent = `◆ ${model}`;
  els.aiOk.textContent = ai?.ok === undefined ? "-" : String(Boolean(ai.ok));
  els.riskHint.textContent = ai?.risk_hint ?? "-";
  els.riskHint.style.color = Number(ai?.risk_hint) >= 6 ? "var(--alarm)" : "var(--warn)";
  els.faceMetric.textContent = faces.length ? (faces[0].identity || "unknown") : "—";
  els.aiSummary.textContent = localizeAiText(ai?.summary) || (event ? "当前事件未包含 AI 摘要。" : "-");
  els.aiExplanation.textContent = explanation;

  const offline = ai?.ok === false;
  els.aiOffline.hidden = !offline;
  els.aiOfflineText.textContent = localizeAiText(ai?.fallback || ai?.error) || "OPi5 AI 服务不可达，本地状态机继续运行。";

  renderImage(imageUrl, panTilt);
  renderLabels(labels);
  renderObjects(objects);
  renderFaces(faces);
  renderLatency(event?.latency_ms || ai?.latency_ms || ai?.vlm_result?.timing);
}

function renderEventStream(events) {
  const rows = (Array.isArray(events) ? events : []).map(normalizeEvent).filter(Boolean);
  els.eventCount.textContent = `${rows.length} 条 · 倒序`;
  if (!rows.length) {
    els.eventsBody.innerHTML = '<tr><td colspan="6" class="empty">暂无事件</td></tr>';
    return;
  }
  els.eventsBody.innerHTML = rows.slice(0, 14).map((event, index) => {
    const state = event.state || "UNKNOWN";
    const color = stateColor(state);
    const risk = event.risk_score ?? 0;
    const summary = localizeAiText(event.ai_result?.summary || event.ai_result?.vlm_result?.description) || "-";
    return `<tr class="${index === 0 ? "fresh" : ""}">
      <td class="c-time">${escapeHtml(shortTime(event.timestamp))}</td>
      <td class="c-seq">${escapeHtml(event.seq ?? "-")}</td>
      <td><span class="spill" style="background:color-mix(in srgb, ${color} 16%, transparent);color:${color}"><span class="spill-dot"></span>${escapeHtml(state)}</span></td>
      <td><span class="rmini"><span class="track"><span class="fill" style="width:${risk * 10}%;background:${risk >= 6 ? "var(--alarm)" : risk >= 3 ? "var(--warn)" : "var(--normal)"}"></span></span>${risk}</span></td>
      <td class="c-sum">${escapeHtml(summary)}</td>
      <td class="c-sens">${escapeHtml(sensorDigest(event.sensors))}</td>
    </tr>`;
  }).join("");
}

function renderDegradeEvidence(event) {
  const aiOffline = isAiOffline(event);
  const flaskSpool = isFlaskSpooling(event);
  const state = event?.state || "UNKNOWN";
  const rows = [
    ["OPi5 不可达 → 跳过 AI，本地继续", aiOffline ? "DEGRADED" : event?.ai_result ? "ONLINE" : "IDLE", aiOffline ? "bad" : "ok"],
    ["Flask 不可达 → spool 落盘补发", flaskSpool ? "SPOOLING" : "ONLINE", flaskSpool ? "warn" : "ok"],
    ["OLED 本地状态屏 · 断网仍刷新", "LOCAL", "ok"],
    ["本地执行器闭环 · 不依赖网络", state === "ALARM" ? "ACTIVE" : "ARMED", "ok"],
  ];
  els.degradeMode.textContent = aiOffline || flaskSpool ? "降级可用" : "本地优先";
  els.spoolBadge.hidden = !flaskSpool;
  els.spoolBadge.textContent = "SPOOL";
  els.degradeList.innerHTML = rows.map(([title, badge, mode]) => {
    const style = modeStyle(mode);
    return `<div class="degrade-row">
      <div class="ic" style="${style}">OK</div>
      <div class="dt">${escapeHtml(title)}<div class="s">本地安全控制优先</div></div>
      <span class="dstate" style="${style}">${escapeHtml(badge)}</span>
    </div>`;
  }).join("");
}

function renderBottomBar(event) {
  const active = event?.state || "UNKNOWN";
  els.scenarioChips.innerHTML = SCENARIO_CHIPS.map(([label, state]) => {
    const color = stateColor(state);
    const isActive = state === active || (active === "UNKNOWN" && label === "NORMAL");
    return `<span class="scen-chip ${isActive ? "active" : ""}" style="${isActive ? `color:${color}` : ""}"><span class="sd" style="background:${color}"></span>${escapeHtml(label)}</span>`;
  }).join("");
}

async function refreshDashboard() {
  try {
    const [latestRes, eventsRes] = await Promise.all([
      fetch("/api/status/latest"),
      fetch("/api/events?limit=20"),
    ]);
    if (!latestRes.ok || !eventsRes.ok) throw new Error(`HTTP ${latestRes.status}/${eventsRes.status}`);
    const latestJson = await latestRes.json();
    const eventsJson = await eventsRes.json();
    if (!latestJson.ok || !eventsJson.ok) throw new Error("API returned ok=false");
    const latest = normalizeEvent(latestJson.event);
    renderDashboard(latest, eventsJson.events || []);
    setError(null);
  } catch (error) {
    setError(`API 请求失败：${error.message}`);
  }
}

function sensorTile(name, pin, value, unit, desc, status, disabled = false) {
  const cls = disabled ? "tile disabled" : status === "hot" ? "tile active" : "tile";
  const badge = disabled ? "SKIPPED" : status === "hot" ? "TRIGGER" : "OK";
  const badgeClass = disabled ? "skip" : status === "hot" ? "hot" : "ok";
  const shown = value === undefined || value === null ? "—" : value;
  return `<div class="${cls}">
    <div class="th"><span class="tname">${escapeHtml(name)}</span><span class="tpin">${escapeHtml(pin)}</span></div>
    <div class="tbody"><div class="tval">${escapeHtml(shown)}${unit ? `<span class="tunit">${escapeHtml(unit)}</span>` : ""}</div><span class="badge ${badgeClass}">${badge}</span></div>
    <div class="tdesc">${escapeHtml(desc)}</div>
  </div>`;
}

function actuatorTile(name, pin, value, desc, badgeText) {
  const active = Number(value) === 1;
  return `<div class="tile ${active ? "active" : ""}">
    <div class="th"><span class="tname">${escapeHtml(name)}</span><span class="tpin">${escapeHtml(pin)}</span></div>
    <div class="tbody"><div class="tval">${active ? "ON" : "OFF"}</div><span class="badge ${active ? "hot" : "ok"}">${escapeHtml(badgeText)}</span></div>
    <div class="tdesc">${escapeHtml(desc)}</div>
  </div>`;
}

function renderImage(imageUrl, panTilt) {
  els.imageLink.textContent = imageUrl ? `image_url: ${imageUrl}` : "image_url: -";
  if (imageUrl) {
    els.imageLink.href = imageUrl;
    els.imageEmpty.hidden = true;
    els.visionImage.hidden = false;
    els.visionImage.src = imageUrl;
    els.visionImage.onerror = () => {
      els.visionImage.hidden = true;
      els.imageEmpty.hidden = false;
      els.imageEmpty.textContent = "图片未找到或无法预览";
    };
  } else {
    els.imageLink.removeAttribute("href");
    els.visionImage.hidden = true;
    els.visionImage.removeAttribute("src");
    els.imageEmpty.hidden = false;
    els.imageEmpty.textContent = "NORMAL — 未触发抓拍";
  }

  const pan = Number(panTilt?.pan_deg);
  const tilt = Number(panTilt?.tilt_deg);
  if (Number.isFinite(pan) || Number.isFinite(tilt)) {
    els.crosshair.hidden = false;
    els.crosshair.style.left = `${clamp(Number.isFinite(pan) ? pan : 90, 0, 180) / 180 * 100}%`;
    els.crosshair.style.top = `${clamp(Number.isFinite(tilt) ? tilt : 90, 0, 180) / 180 * 100}%`;
  } else {
    els.crosshair.hidden = true;
  }
}

function renderLabels(labels) {
  els.labelsList.innerHTML = Array.isArray(labels) && labels.length
    ? labels.map((label) => `<span class="lchip">${escapeHtml(label)}</span>`).join("")
    : '<span class="lchip">-</span>';
}

function renderObjects(objects) {
  if (!Array.isArray(objects) || !objects.length) {
    els.objectsList.textContent = "objects[] = [] — Qwen3-VL 为视觉语言模型，不输出像素级 bbox。目标以 labels 文本呈现，不伪造 bbox。";
    return;
  }
  els.objectsList.innerHTML = objects.map((item) => {
    const bbox = Array.isArray(item?.bbox) ? item.bbox.join(",") : "-";
    const score = item?.score === undefined ? "-" : Number(item.score).toFixed(2);
    return `${escapeHtml(item?.label || "unknown")} · ${escapeHtml(score)} · bbox:${escapeHtml(bbox)}`;
  }).join("<br>");
}

function renderFaces(faces) {
  els.facesList.textContent = Array.isArray(faces) && faces.length
    ? `faces: ${faces.map((face) => `${face.identity || "unknown"} ${face.score ?? "-"}`).join(" / ")}`
    : "faces: -";
}

function renderLatency(latency) {
  const value = objectOrEmpty(latency);
  const total = ["capture", "ai", "post"].reduce((sum, key) => sum + (Number(value[key]) || 0), 0);
  const parts = [
    ["capture", value.capture],
    ["ai infer", value.ai ?? value.infer_ms],
    ["post", value.post],
    ["total", value.total ?? total],
  ];
  els.latencyStrip.innerHTML = parts.map(([key, val], index) =>
    `<div class="seg ${index === 3 ? "total" : ""}"><div class="k">${escapeHtml(key)}</div><div class="v">${val ?? "—"}${val == null ? "" : "ms"}</div></div>`
  ).join("");
}

function setClock() {
  els.clockTime.textContent = new Date().toTimeString().slice(0, 8);
}

function setError(message) {
  els.errorBox.hidden = !message;
  els.errorBox.textContent = message || "";
  if (message) setDot(els.flaskDot, "down");
}

function localizeAiText(text) {
  if (!text) return "";
  const raw = String(text);
  const normalized = raw.toLowerCase();
  if (normalized.includes("opi5") && normalized.includes("unavailable")) {
    return "OPi5 AI 服务不可达；当前仅使用 i.MX6ULL 本地安全逻辑。";
  }
  if (normalized.includes("opi5_unreachable")) {
    return "OPi5 不可达；AI 降级，本地状态机继续运行。";
  }
  if (normalized.includes("local safety logic only")) {
    return "仅使用本地安全逻辑。";
  }
  return raw;
}

function setDot(el, state) {
  el.className = `dot ${state}`;
}

function objectOrEmpty(value) {
  return value && typeof value === "object" && !Array.isArray(value) ? value : {};
}

function objectOrNull(value) {
  return value && typeof value === "object" && !Array.isArray(value) ? value : null;
}

function isAiOffline(event) {
  const ai = event?.ai_result;
  return ai?.ok === false || String(event?.ai_status || ai?.mode || "").toLowerCase().includes("offline");
}

function isFlaskSpooling(event) {
  return String(event?.backend_status || "").toLowerCase().includes("spool");
}

function modeStyle(mode) {
  if (mode === "bad") return "background:rgba(255,77,94,0.16);color:var(--alarm)";
  if (mode === "warn") return "background:rgba(245,181,68,0.16);color:var(--warn)";
  return "background:rgba(52,224,161,0.14);color:var(--normal)";
}

function stateClass(state) {
  return {
    NORMAL: "state-normal",
    WARN: "state-warn",
    VERIFY: "state-verify",
    ALARM: "state-alarm",
    FAULT: "state-fault",
  }[state] || "state-unknown";
}

function stateColor(state) {
  return (STATE_META[state] || STATE_META.UNKNOWN).color;
}

function sensorDigest(sensors = {}) {
  const parts = [`pir:${sensors.pir ?? "-"}`, `flame:${sensors.flame ?? "-"}`, `mq2:${sensors.mq2 ?? "-"}`];
  if (sensors.soc_temp !== undefined && sensors.soc_temp !== null) parts.push(`soc:${sensors.soc_temp}°C`);
  return parts.join(" ");
}

function rgbColor(a = {}, state) {
  if (Number(a.rgb_r) && Number(a.rgb_b)) return "var(--fault)";
  if (Number(a.rgb_r)) return "var(--alarm)";
  if (Number(a.rgb_b)) return "var(--verify)";
  if (Number(a.rgb_g)) return "var(--normal)";
  return stateColor(state);
}

function shortTime(timestamp) {
  if (!timestamp) return "-";
  const parsed = new Date(timestamp);
  if (Number.isNaN(parsed.getTime())) return String(timestamp).slice(11, 19) || timestamp;
  return parsed.toTimeString().slice(0, 8);
}

function clamp(value, min, max) {
  if (!Number.isFinite(value)) return min;
  return Math.max(min, Math.min(max, value));
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (char) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#039;",
  }[char]));
}

window.addEventListener("resize", fitStage);
fitStage();
setClock();
setInterval(setClock, 1000);
renderBottomBar(null);
refreshDashboard();
setInterval(refreshDashboard, 3000);
