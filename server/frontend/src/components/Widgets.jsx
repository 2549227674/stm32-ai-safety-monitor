/* ============================================================
   widgets.jsx — 通用组件：useSim、Card、Pill、视频面、空态等
   ============================================================ */
import React from 'react'

function useSim() {
  const [, force] = React.useState(0);
  React.useEffect(() => {
    const un = window.EdgeSim.subscribe(() => force((x) => x + 1));
    return un;
  }, []);
  return window.EdgeSim;
}

/* ---------- helpers ---------- */
function riskLevel(v) {
  const t = window.EdgeSim.thresholds.risk_score;
  if (v == null) return "off";
  if (v >= t.danger) return "danger";
  if (v >= t.warn) return "warn";
  return "ok";
}
function riskColor(lvl) {
  return { ok: "var(--ok)", warn: "var(--warn)", danger: "var(--danger)", off: "var(--off)" }[lvl] || "var(--off)";
}
function fmtUptime(s) {
  if (s == null) return "—";
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return (d ? d + "d " : "") + h + "h " + m + "m";
}
function timeAgo(ts) {
  if (!ts) return "—";
  const s = Math.floor(Date.now() / 1000) - ts;
  if (s < 3) return "刚刚";
  if (s < 60) return s + "s 前";
  if (s < 3600) return Math.floor(s / 60) + "m 前";
  return Math.floor(s / 3600) + "h 前";
}
function fmtTime(ts) {
  const d = new Date(ts * 1000);
  const p = (n) => String(n).padStart(2, "0");
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

const LEVEL_LABEL = { info: "INFO", warn: "WARN", danger: "ALERT" };

/* ---------- atoms ---------- */
function Card({ title, sub, right, children, flush, style, anchor }) {
  return (
    <section className="card" style={style} data-comment-anchor={anchor}>
      {(title || right) && (
        <header className="card-hd">
          <h3>{title}</h3>
          {sub && <span className="sub">{sub}</span>}
          <span className="spacer"></span>
          {right}
        </header>
      )}
      <div className={"card-bd" + (flush ? " flush" : "")}>{children}</div>
    </section>
  );
}

function Pill({ level = "off", children, blink }) {
  return (
    <span className={"pill " + level}>
      <span className={"dot" + (blink ? " blink" : "")}></span>
      {children}
    </span>
  );
}

function Tag({ level = "", children }) {
  return <span className={"tag " + level}>{children}</span>;
}

function Empty({ icon, children }) {
  return (
    <div className="empty">
      <span className="ic">{icon || "○"}</span>
      <span>{children}</span>
    </div>
  );
}

/* ---------- scenario → 全局状态推导 ---------- */
function systemStatus(sim) {
  const s = sim.scenario;
  const risk = sim.latest("risk");
  if (s === "offline") return { level: "off", label: "设备离线", pill: "danger", blink: true };
  if (s === "alarm" || riskLevel(risk) === "danger") return { level: "danger", label: "告警", pill: "danger", blink: true };
  if (s === "degraded") return { level: "degraded", label: "降级运行", pill: "degraded", blink: false };
  if (riskLevel(risk) === "warn") return { level: "warn", label: "需关注", pill: "warn", blink: false };
  return { level: "ok", label: "运行正常", pill: "ok", blink: false };
}

/* ---------- 视频面（image-slot + HUD + 状态层） ---------- */
function VideoSurface({ sim }) {
  const scen = sim.scenario;
  const risk = sim.latest("risk");
  const lvl = riskLevel(risk);
  const [clock, setClock] = React.useState("");
  React.useEffect(() => {
    const tick = () => {
      const d = new Date();
      const p = (n) => String(n).padStart(2, "0");
      setClock(`${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`);
    };
    tick();
    const t = setInterval(tick, 1000);
    return () => clearInterval(t);
  }, []);

  const offline = scen === "offline";
  const degraded = scen === "degraded";
  const fps = offline ? 0 : degraded ? 6 : 25;
  const ai = sim.ai.find((o) => o.ok);

  return (
    <div className="video-wrap" style={{ aspectRatio: "16 / 9" }}>
      <image-slot
        id="live-frame-main"
        shape="rect"
        placeholder="拖入一张摄像头画面截图（演示帧）"
      ></image-slot>
      <div className="scanline"></div>

      {offline && (
        <div className="video-state">
          <span style={{ fontFamily: "var(--mono)", fontSize: 11, letterSpacing: "0.2em", color: "var(--danger)" }}>NO SIGNAL</span>
          <span>视频流断开 · 等待设备心跳恢复</span>
          <span className="t3" style={{ fontSize: 11.5, fontFamily: "var(--mono)" }}>GET /api/video/stream → 503</span>
        </div>
      )}

      <div className="hud">
        <div className="hud-row">
          <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
            <span className="hud-chip">
              <span className="dot" style={{ background: offline ? "var(--off)" : "var(--danger)", boxShadow: offline ? "none" : "0 0 5px var(--danger)" }}></span>
              {offline ? "OFFLINE" : "LIVE"} · CAM-01
            </span>
            <span className="hud-chip">{fps} FPS · MJPEG 1280×720</span>
          </div>
          <span className="hud-chip">{clock}</span>
        </div>
        <div className="hud-row" style={{ alignItems: "flex-end" }}>
          <span className="hud-chip">
            <span className="dot" style={{ background: riskColor(lvl) }}></span>
            risk {risk == null ? "—" : risk.toFixed(1)}/10
          </span>
          {!offline && ai && (
            <span className="hud-chip" style={{ maxWidth: "62%", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", display: "inline-block", lineHeight: "18px" }}>
              AI · {ai.status === "mock" ? "[mock] " : ""}{(ai.summary || "").slice(0, 38)}…
            </span>
          )}
        </div>
      </div>
    </div>
  );
}

/* ---------- AI 观察列表项 ---------- */
function ObservationRow({ obs, onClick, active }) {
  const fail = !obs.ok;
  const mock = obs.status === "mock";
  const lvl = fail ? "warn" : mock ? "degraded" : obs.risk_hint >= 7 ? "danger" : obs.risk_hint >= 4 ? "warn" : "ok";
  return (
    <button
      onClick={onClick}
      style={{
        display: "grid", gridTemplateColumns: "64px 52px 1fr auto", gap: 10, alignItems: "baseline",
        width: "100%", textAlign: "left", font: "inherit", cursor: "pointer",
        background: active ? "var(--raised)" : "transparent",
        border: 0, borderBottom: "1px solid var(--line)",
        padding: "9px 16px", color: "var(--text-2)",
      }}
    >
      <span className="mono t3" style={{ fontSize: 11 }}>{fmtTime(obs.timestamp)}</span>
      {fail ? (
        <Tag level="warn">{obs.status === "timeout" ? "超时" : "失败"}</Tag>
      ) : mock ? (
        <Tag level="degraded">mock</Tag>
      ) : (
        <Tag level={lvl}>{obs.risk_hint}/10</Tag>
      )}
      <span style={{ minWidth: 0, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", fontSize: 12.5 }}>
        {fail ? obs.error : obs.summary}
      </span>
      <span className="mono t3" style={{ fontSize: 10.5 }}>
        {obs.run_metrics ? (obs.run_metrics.total_ms / 1000).toFixed(1) + "s" : "—"}
      </span>
    </button>
  );
}

export {
  useSim, riskLevel, riskColor, fmtUptime, timeAgo, fmtTime, LEVEL_LABEL,
  Card, Pill, Tag, Empty, systemStatus, VideoSurface, ObservationRow,
};
