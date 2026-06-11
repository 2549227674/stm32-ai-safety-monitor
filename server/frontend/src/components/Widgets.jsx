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
  // real 模式：优先看设备状态
  if (sim.mode === "real") {
    const dev = sim.device;
    if (!dev || !dev.online) return { level: "off", label: "设备离线", pill: "danger", blink: true };
    const aiOk = dev.ai && dev.ai.ok;
    const cs = sim.cameraStatus;
    const cameraOk = sim.videoOnline && (cs === "online" || cs === "mock");
    if (!aiOk && !cameraOk) return { level: "degraded", label: "降级运行", pill: "degraded", blink: false };
    if (!cameraOk) return { level: "warn", label: "视频异常", pill: "warn", blink: false };
    const risk = sim.latest("risk");
    if (riskLevel(risk) === "danger") return { level: "danger", label: "告警", pill: "danger", blink: true };
    if (riskLevel(risk) === "warn") return { level: "warn", label: "需关注", pill: "warn", blink: false };
    return { level: "ok", label: "运行正常", pill: "ok", blink: false };
  }
  // mock 模式：看 scenario
  const s = sim.scenario;
  const risk = sim.latest("risk");
  if (s === "offline") return { level: "off", label: "设备离线", pill: "danger", blink: true };
  if (s === "alarm" || riskLevel(risk) === "danger") return { level: "danger", label: "告警", pill: "danger", blink: true };
  if (s === "degraded") return { level: "degraded", label: "降级运行", pill: "degraded", blink: false };
  if (riskLevel(risk) === "warn") return { level: "warn", label: "需关注", pill: "warn", blink: false };
  return { level: "ok", label: "运行正常", pill: "ok", blink: false };
}

/* ---------- 视频面（real MJPEG / mock 占位） ---------- */
const StableVideoImg = React.memo(function StableVideoImg({ src, onError }) {
  return (
    <img
      src={src}
      alt="CAM-01 实时流"
      style={{ width: "100%", height: "100%", objectFit: "contain", display: "block" }}
      onError={onError}
    />
  );
}, (prev, next) => prev.src === next.src);

function VideoSurface({ sim }) {
  const isReal = sim.mode === "real";
  const risk = sim.latest("risk");
  const lvl = riskLevel(risk);
  const [clock, setClock] = React.useState("");
  const [imgError, setImgError] = React.useState(false);

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

  // real 模式状态判断
  const dev = sim.device;
  const deviceOnline = isReal ? (dev && dev.online) : (sim.scenario !== "offline");
  const cameraStatus = isReal ? sim.cameraStatus : (sim.scenario === "offline" ? "offline" : "mock");
  const videoMode = isReal ? (dev && dev.video_mode) : (sim.scenario === "offline" ? "offline" : "mock");
  const cameraOk = isReal ? (sim.videoOnline && (cameraStatus === "online" || cameraStatus === "mock")) : deviceOnline;
  const isMockCam = cameraStatus === "mock" || videoMode === "mock";
  const isRealCam = cameraStatus === "online" && videoMode === "real";
  const videoSrc = isReal && deviceOnline && cameraOk
    ? `/api/video/stream?device_id=${sim._realDeviceId || "edge-opi5-001"}`
    : null;

  // Clear imgError when device_id or videoSrc changes
  React.useEffect(() => {
    setImgError(false);
  }, [sim._realDeviceId, videoSrc]);

  const showNoSignal = !deviceOnline || (isReal && !cameraOk);
  const showMockPlaceholder = !isReal && sim.scenario !== "offline";

  return (
    <div className="video-wrap" style={{ aspectRatio: "16 / 9" }}>
      {/* 真实模式：MJPEG 流 */}
      {isReal && deviceOnline && cameraOk && !imgError && (
        <StableVideoImg src={videoSrc} onError={() => setImgError(true)} />
      )}

      {/* mock 模式：image-slot 占位 */}
      {!isReal && (
        <image-slot
          id="live-frame-main"
          shape="rect"
          placeholder="拖入一张摄像头画面截图（演示帧）"
        ></image-slot>
      )}

      <div className="scanline"></div>

      {/* 无信号状态 */}
      {showNoSignal && (
        <div className="video-state">
          <span style={{ fontFamily: "var(--mono)", fontSize: 11, letterSpacing: "0.2em", color: "var(--danger)" }}>NO SIGNAL</span>
          {isReal ? (
            <React.Fragment>
              <span>{!deviceOnline ? "设备离线 · 等待心跳恢复" : "视频流未连接 · 等待摄像头"}</span>
              <span className="t3" style={{ fontSize: 11.5, fontFamily: "var(--mono)" }}>
                {sim.videoError ? `错误: ${sim.videoError}` : "GET /api/video/stream → 等待"}
              </span>
            </React.Fragment>
          ) : (
            <React.Fragment>
              <span>视频流断开 · 等待设备心跳恢复</span>
              <span className="t3" style={{ fontSize: 11.5, fontFamily: "var(--mono)" }}>mock 模式 · 设备离线场景</span>
            </React.Fragment>
          )}
        </div>
      )}

      {/* 图片加载失败 */}
      {isReal && deviceOnline && cameraOk && imgError && (
        <div className="video-state">
          <span style={{ fontFamily: "var(--mono)", fontSize: 11, letterSpacing: "0.2em", color: "var(--warn)" }}>STREAM ERROR</span>
          <span>视频流加载失败 · 设备可能未推流</span>
          <button className="btn" style={{ marginTop: 8 }} onClick={() => setImgError(false)}>重试</button>
        </div>
      )}

      {/* HUD */}
      <div className="hud">
        <div className="hud-row">
          <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
            <span className="hud-chip">
              <span className="dot" style={{ background: !deviceOnline ? "var(--off)" : cameraOk ? "var(--ok)" : "var(--warn)", boxShadow: deviceOnline && cameraOk ? "0 0 5px var(--ok)" : "none" }}></span>
              {!deviceOnline ? "OFFLINE" : cameraOk ? "LIVE" : "NO CAM"} · CAM-01
            </span>
            {isReal && <span className="hud-chip">{cameraOk && !imgError ? "MJPEG" : "断流"}</span>}
            {!isReal && <span className="hud-chip">25 FPS · MJPEG 1280×720 · mock</span>}
            {isReal && isMockCam && <span className="hud-chip" style={{ color: "var(--warn)", fontWeight: 600 }}>MOCK VIDEO</span>}
            {isReal && isRealCam && <span className="hud-chip" style={{ color: "var(--ok)", fontWeight: 600 }}>REAL CAMERA</span>}
          </div>
          <span className="hud-chip">{clock}</span>
        </div>
        <div className="hud-row" style={{ alignItems: "flex-end" }}>
          <span className="hud-chip">
            <span className="dot" style={{ background: riskColor(lvl) }}></span>
            risk {risk == null ? "—" : risk.toFixed(1)}/10
          </span>
          {isReal && isRealCam && (
            <span className="hud-chip" style={{ fontSize: 9, color: "var(--ok)" }}>REAL</span>
          )}
          {isReal && isMockCam && (
            <span className="hud-chip" style={{ fontSize: 9, color: "var(--warn)" }}>MOCK</span>
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
