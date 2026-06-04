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
    els.eventsBody.innerHTML = '<tr><td colspan="5" class="empty">暂无事件</td></tr>';
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
        <td>${escapeHtml(sensors)}</td>
      </tr>`;
    })
    .join("");
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
  return value.replace(/[&<>"']/g, (char) => {
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
