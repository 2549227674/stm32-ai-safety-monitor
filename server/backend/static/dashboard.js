const sensorKeys = ["door", "pir", "flame", "mq2"];
const actuatorKeys = ["relay", "fan", "pump", "buzzer", "rgb_r", "rgb_g", "rgb_b"];

const els = {
  refreshText: document.getElementById("refreshText"),
  apiState: document.getElementById("apiState"),
  stateBadge: document.getElementById("stateBadge"),
  currentState: document.getElementById("currentState"),
  riskScore: document.getElementById("riskScore"),
  deviceId: document.getElementById("deviceId"),
  seq: document.getElementById("seq"),
  timestamp: document.getElementById("timestamp"),
  aiState: document.getElementById("aiState"),
  aiEmpty: document.getElementById("aiEmpty"),
  aiDetails: document.getElementById("aiDetails"),
  aiOk: document.getElementById("aiOk"),
  riskHint: document.getElementById("riskHint"),
  controlAllowed: document.getElementById("controlAllowed"),
  panTilt: document.getElementById("panTilt"),
  aiSummary: document.getElementById("aiSummary"),
  latencyMs: document.getElementById("latencyMs"),
  objectsList: document.getElementById("objectsList"),
  facesList: document.getElementById("facesList"),
  imageBox: document.getElementById("imageBox"),
  imageLink: document.getElementById("imageLink"),
  visionImage: document.getElementById("visionImage"),
  imageMissing: document.getElementById("imageMissing"),
  sensorGrid: document.getElementById("sensorGrid"),
  actuatorGrid: document.getElementById("actuatorGrid"),
  eventCount: document.getElementById("eventCount"),
  errorBox: document.getElementById("errorBox"),
  eventsBody: document.getElementById("eventsBody"),
};

function stateClass(state) {
  const normalized = String(state || "unknown").toLowerCase().replace(/_/g, "-");
  if (["idle", "normal"].includes(normalized)) return `state-${normalized}`;
  if (["pir", "door", "pre-alarm"].includes(normalized)) return `state-${normalized}`;
  if (["smoke", "alarm", "danger", "flame"].includes(normalized)) return `state-${normalized}`;
  if (normalized === "test") return "state-test";
  return "state-unknown";
}

function setApiState(ok, message = "") {
  els.apiState.className = ok ? "api-state ok" : "api-state error";
  els.apiState.textContent = ok ? "API OK" : "API ERROR";
  els.errorBox.hidden = ok;
  els.errorBox.textContent = message;
}

function renderLatest(event) {
  if (!event) {
    els.currentState.textContent = "暂无事件";
    els.riskScore.textContent = "-";
    els.deviceId.textContent = "-";
    els.seq.textContent = "-";
    els.timestamp.textContent = "-";
    els.stateBadge.textContent = "UNKNOWN";
    els.stateBadge.className = "state-badge state-unknown";
    renderChips(els.sensorGrid, sensorKeys, {});
    renderChips(els.actuatorGrid, actuatorKeys, {});
    renderAiPanel(null);
    return;
  }

  els.currentState.textContent = event.state || "UNKNOWN";
  els.riskScore.textContent = event.risk_score ?? 0;
  els.deviceId.textContent = event.device_id || "unknown";
  els.seq.textContent = event.seq ?? "-";
  els.timestamp.textContent = event.timestamp || "-";
  els.stateBadge.textContent = event.state || "UNKNOWN";
  els.stateBadge.className = `state-badge ${stateClass(event.state)}`;
  renderChips(els.sensorGrid, sensorKeys, event.sensors || {});
  renderChips(els.actuatorGrid, actuatorKeys, event.actuators || {});
  renderAiPanel(event);
}

function renderAiPanel(event) {
  const ai = event?.ai_result || null;
  const vision = event?.vision || null;
  const imageUrl = event?.image_url || vision?.image_url || "";
  const hasAiResult = Boolean(ai || vision || imageUrl || event?.latency_ms);

  els.aiEmpty.hidden = hasAiResult;
  els.aiDetails.hidden = !hasAiResult;
  els.aiState.textContent = hasAiResult ? "已接收 AI/视觉结果" : "暂无 AI/视觉结果";

  if (!hasAiResult) {
    els.aiOk.textContent = "-";
    els.riskHint.textContent = "-";
    els.controlAllowed.textContent = "false";
    els.panTilt.textContent = "-";
    els.aiSummary.textContent = "-";
    els.latencyMs.textContent = "-";
    els.objectsList.textContent = "无目标";
    els.facesList.textContent = "无人脸结果";
    renderImage("", false);
    return;
  }

  const controlAllowed = ai?.control_allowed === true;
  const panTilt = vision?.pan_tilt || {};
  els.aiOk.textContent = ai?.ok === undefined ? "-" : String(Boolean(ai.ok));
  els.riskHint.textContent = ai?.risk_hint ?? "-";
  els.controlAllowed.textContent = String(controlAllowed);
  els.controlAllowed.className = controlAllowed ? "unsafe-control" : "safe-control";
  els.panTilt.textContent =
    panTilt.pan_deg === undefined && panTilt.tilt_deg === undefined
      ? "-"
      : `${panTilt.pan_deg ?? "-"} / ${panTilt.tilt_deg ?? "-"}`;
  els.aiSummary.textContent = ai?.summary || "-";
  els.latencyMs.textContent = formatLatency(event.latency_ms);
  renderObjectList(els.objectsList, ai?.objects || [], "无目标");
  renderFaceList(els.facesList, ai?.faces || []);
  renderImage(imageUrl, Boolean(imageUrl));
}

function renderChips(container, keys, data) {
  container.innerHTML = keys
    .map((key) => {
      const value = data[key] ?? 0;
      const active = Number(value) ? "active" : "";
      const danger = ["flame", "mq2", "pump", "buzzer", "rgb_r"].includes(key) && Number(value) ? "danger" : "";
      return `<div class="chip ${active} ${danger}">
        <span>${escapeHtml(key.toUpperCase())}</span>
        <strong>${escapeHtml(String(value))}</strong>
      </div>`;
    })
    .join("");
}

function renderEvents(events) {
  els.eventCount.textContent = String(events.length);
  if (!events.length) {
    els.eventsBody.innerHTML = '<tr><td colspan="6" class="empty">暂无事件</td></tr>';
    return;
  }

  els.eventsBody.innerHTML = events
    .map((event) => {
      const sensors = sensorKeys
        .map((key) => `${key}:${event.sensors?.[key] ?? 0}`)
        .join(" ");
      return `<tr>
        <td>${escapeHtml(event.timestamp || "-")}</td>
        <td>${escapeHtml(event.device_id || "unknown")}</td>
        <td><span class="state-badge ${stateClass(event.state)}">${escapeHtml(event.state || "UNKNOWN")}</span></td>
        <td>${escapeHtml(String(event.risk_score ?? 0))}</td>
        <td>${renderAiSummaryCell(event)}</td>
        <td>${escapeHtml(sensors)}</td>
      </tr>`;
    })
    .join("");
}

function renderAiSummaryCell(event) {
  const ai = event?.ai_result;
  if (!ai) return '<span class="subtle">-</span>';
  const risk = ai.risk_hint ?? "-";
  const summary = ai.summary || "AI result";
  return `<span class="ai-risk">R${escapeHtml(risk)}</span> ${escapeHtml(summary)}`;
}

function renderObjectList(container, objects, emptyText) {
  if (!Array.isArray(objects) || !objects.length) {
    container.className = "list-box empty";
    container.textContent = emptyText;
    return;
  }
  container.className = "list-box";
  container.innerHTML = objects
    .map((item) => {
      const label = item?.label || "unknown";
      const score = item?.score === undefined ? "-" : Number(item.score).toFixed(2);
      return `<div class="list-row">
        <strong>${escapeHtml(label)}</strong>
        <span>${escapeHtml(score)}</span>
      </div>`;
    })
    .join("");
}

function renderFaceList(container, faces) {
  if (!Array.isArray(faces) || !faces.length) {
    container.className = "list-box empty";
    container.textContent = "无人脸结果";
    return;
  }
  container.className = "list-box";
  container.innerHTML = faces
    .map((item) => {
      const identity = item?.identity || "unknown";
      const score = item?.score === undefined ? "-" : Number(item.score).toFixed(2);
      return `<div class="list-row">
        <strong>${escapeHtml(identity)}</strong>
        <span>${escapeHtml(score)}</span>
      </div>`;
    })
    .join("");
}

function renderImage(imageUrl, shouldShow) {
  els.imageBox.hidden = !shouldShow;
  els.visionImage.hidden = true;
  els.imageMissing.hidden = true;

  if (!shouldShow) {
    els.imageLink.removeAttribute("href");
    els.visionImage.removeAttribute("src");
    return;
  }

  els.imageLink.href = imageUrl;
  els.imageLink.textContent = `打开图片：${imageUrl}`;
  els.visionImage.onload = () => {
    els.visionImage.hidden = false;
    els.imageMissing.hidden = true;
  };
  els.visionImage.onerror = () => {
    els.visionImage.hidden = true;
    els.imageMissing.hidden = false;
  };
  els.visionImage.src = imageUrl;
}

function formatLatency(latency) {
  if (!latency) return "-";
  if (typeof latency === "number") return `${latency}ms`;
  if (typeof latency !== "object") return String(latency);
  return ["capture", "ai", "post"]
    .filter((key) => latency[key] !== undefined)
    .map((key) => `${key}:${latency[key]}ms`)
    .join(" ") || "-";
}

async function refreshDashboard() {
  try {
    const [latestRes, eventsRes] = await Promise.all([
      fetch("/api/status/latest"),
      fetch("/api/events?limit=20"),
    ]);

    if (!latestRes.ok || !eventsRes.ok) {
      throw new Error(`HTTP ${latestRes.status}/${eventsRes.status}`);
    }

    const latestJson = await latestRes.json();
    const eventsJson = await eventsRes.json();
    if (!latestJson.ok || !eventsJson.ok) {
      throw new Error("API returned ok=false");
    }

    renderLatest(latestJson.event);
    renderEvents(eventsJson.events || []);
    setApiState(true);
    els.refreshText.textContent = `最近刷新：${new Date().toLocaleTimeString()}`;
  } catch (error) {
    setApiState(false, `API 请求失败：${error.message}`);
  }
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (char) => {
    const map = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#039;",
    };
    return map[char];
  });
}

refreshDashboard();
setInterval(refreshDashboard, 3000);
