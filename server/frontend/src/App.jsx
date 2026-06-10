import React from 'react'
import { useSim, Pill, systemStatus } from './components/Widgets'
import PageOverview from './pages/Overview'
import PageLive from './pages/Live'
import PageAI from './pages/AI'
import PageSensors from './pages/Sensors'
import PageThresholds from './pages/Thresholds'
import PageAlerts from './pages/Alerts'
import PageNotify from './pages/Notify'
import PageDevice from './pages/Device'

const NAV = [
  { group: "监控" },
  { id: "overview", label: "总览", icon: "M3 3h7v7H3zM14 3h7v4h-7zM14 11h7v10h-7zM3 14h7v7H3z" },
  { id: "live", label: "实时巡检", icon: "M4 6h16v10H4zM9 20h6M12 16v4" },
  { id: "ai", label: "AI 推理", icon: "M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2M8.5 8.5h7v7h-7z" },
  { id: "sensors", label: "传感器", icon: "M3 17l4-6 4 3 4-8 6 11" },
  { group: "管理" },
  { id: "thresholds", label: "风险阈值", icon: "M4 7h10M18 7h2M4 17h2M10 17h10M14 4.5v5M8 14.5v5" },
  { id: "alerts", label: "事件与告警", icon: "M12 3l9 16H3zM12 10v4M12 17.5v.5" },
  { id: "notify", label: "通知设置", icon: "M3 6h18v12H3zM3 7l9 6 9-6" },
  { id: "device", label: "设备健康", icon: "M7 3h10v18H7zM10 7h4M12 16v.5" },
];

const PAGES = {
  overview: { title: "总览", comp: (p) => <PageOverview {...p} /> },
  live: { title: "实时巡检", comp: (p) => <PageLive {...p} /> },
  ai: { title: "AI 推理", comp: (p) => <PageAI {...p} /> },
  sensors: { title: "传感器", comp: (p) => <PageSensors {...p} /> },
  thresholds: { title: "风险阈值", comp: (p) => <PageThresholds {...p} /> },
  alerts: { title: "事件与告警", comp: (p) => <PageAlerts {...p} /> },
  notify: { title: "通知设置", comp: (p) => <PageNotify {...p} /> },
  device: { title: "设备健康", comp: (p) => <PageDevice {...p} /> },
};

const SCENARIO_OPTS = [
  { v: "normal", l: "正常巡检" },
  { v: "alarm", l: "火灾告警" },
  { v: "degraded", l: "AI 降级" },
  { v: "offline", l: "设备离线" },
];

function Icon({ d }) {
  return (
    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round">
      {d.split("M").filter(Boolean).map((seg, i) => <path key={i} d={"M" + seg} />)}
    </svg>
  );
}

function Clock() {
  const [s, setS] = React.useState("");
  React.useEffect(() => {
    const f = () => { const d = new Date(); const p = (n) => String(n).padStart(2, "0"); setS(`${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`); };
    f(); const t = setInterval(f, 1000); return () => clearInterval(t);
  }, []);
  return <span className="clock">{s}</span>;
}

export default function App() {
  const sim = useSim();
  const [scenario, setScenario] = React.useState("normal");
  const [page, setPage] = React.useState(() => {
    const h = location.hash.slice(1);
    return h && h in PAGES ? h : "overview";
  });

  React.useEffect(() => { window.EdgeSim.setScenario(scenario); }, [scenario]);
  React.useEffect(() => {
    const onHash = () => { const h = location.hash.slice(1); if (PAGES[h]) setPage(h); };
    window.addEventListener("hashchange", onHash);
    return () => window.removeEventListener("hashchange", onHash);
  }, []);

  const nav = (id) => { location.hash = id; setPage(id); window.scrollTo({ top: 0 }); };
  const st = systemStatus(sim);
  const dangerCount = sim.alerts.filter((a) => a.level === "danger" && Date.now() / 1000 - a.ts < 600).length;
  const P = PAGES[page];

  return (
    <div className="shell">
      <aside className="sidebar">
        <div className="brand">
          <span className="brand-mark">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <path d="M12 2L3 7v6c0 5 4 8.4 9 9 5-.6 9-4 9-9V7l-9-5z" />
              <path d="M8.5 12.5l2.5 2.5 4.5-5" />
            </svg>
          </span>
          <div>
            <div className="brand-name">边缘 AI 巡检</div>
            <div className="brand-sub">EDGE SAFETY CONSOLE</div>
          </div>
        </div>
        <div className="device-chip">
          <span className="dot" style={{ width: 7, height: 7, borderRadius: "50%", flex: "none", background: sim.mode === "real" ? "var(--ok)" : sim.scenario === "offline" ? "var(--danger)" : "var(--warn)", boxShadow: sim.mode === "real" ? "0 0 5px var(--ok)" : "none" }}></span>
          <span className="grow">{sim.mode === "real" ? (sim._realDeviceId || "edge-opi5-001") : "edge-opi5-001"}</span>
          <span style={{ color: sim.mode === "real" ? "var(--ok)" : "var(--warn)", fontSize: 10 }}>{sim.mode === "real" ? "REAL" : "MOCK"}</span>
        </div>
        <nav className="nav">
          {NAV.map((n, i) => n.group ? (
            <div key={i} className="nav-group">{n.group}</div>
          ) : (
            <a key={n.id} className={"nav-item" + (page === n.id ? " active" : "")} href={"#" + n.id} onClick={(e) => { e.preventDefault(); nav(n.id); }}>
              <span className="ni-icon"><Icon d={n.icon} /></span>
              <span className="lbl">{n.label}</span>
              {n.id === "alerts" && dangerCount > 0 && <span className="ni-badge">{dangerCount}</span>}
            </a>
          ))}
        </nav>
        <div className="sidebar-foot">
          <div className="linkrow"><b>backend</b> Flask · SQLite</div>
          <div className="linkrow"><b>contract</b> v1.1 · 07 Contract</div>
          <div className="linkrow"><b>agent</b> 0.2.0 · OPi5 RK3588S</div>
        </div>
      </aside>
      <div className="main">
        <header className="topbar">
          <h1>{P.title}</h1>
          <span className="crumb">边缘 AI 安全巡检 / {P.title}</span>
          <span className="spacer"></span>
          {sim.mode !== "real" && (
            <div className="seg" style={{ marginRight: 8 }}>
              {SCENARIO_OPTS.map((o) => (
                <button key={o.v} className={scenario === o.v ? "on" : ""} onClick={() => setScenario(o.v)}>{o.l}</button>
              ))}
            </div>
          )}
          {sim.mode === "real" && (
            <span className="tag ok" style={{ marginRight: 8 }}>真实设备</span>
          )}
          <Pill level={st.pill} blink={st.blink}>{st.label}</Pill>
          <Clock />
        </header>
        <nav className="mobilenav">
          {NAV.filter((n) => n.id).map((n) => (
            <a key={n.id} className={page === n.id ? "active" : ""} href={"#" + n.id} onClick={(e) => { e.preventDefault(); nav(n.id); }}>{n.label}</a>
          ))}
        </nav>
        <main className="content">{P.comp({ sim, nav })}</main>
      </div>
    </div>
  );
}
