import React from 'react'
import { Card, riskColor } from '../components/Widgets'

export default function PageThresholds({ sim }) {
  const T = sim.thresholds;
  const isReal = sim.mode === "real";
  const [draft, setDraft] = React.useState(() => {
    const d = {};
    Object.keys(T).forEach((k) => (d[k] = { warn: T[k].warn, danger: T[k].danger }));
    return d;
  });
  const [savedAt, setSavedAt] = React.useState(null);
  const [saving, setSaving] = React.useState(false);
  const [error, setError] = React.useState(null);
  const dirty = Object.keys(T).some((k) => Number(draft[k].warn) !== T[k].warn || Number(draft[k].danger) !== T[k].danger);
  const cur = { risk_score: sim.latest("risk"), cpu_temp_c: sim.latest("cpu_temp"), "mpu6500.vibration_score": sim.latest("vib"), smoke_score: sim.latest("smoke_score") };

  // 加载阈值
  React.useEffect(() => {
    if (!isReal) return;
    const deviceId = sim._realDeviceId || "default";
    fetch(`/api/thresholds?device_id=${deviceId}`)
      .then((r) => r.json())
      .then((data) => {
        if (data.ok && data.thresholds) {
          Object.keys(data.thresholds).forEach((k) => {
            if (T[k]) {
              T[k].warn = data.thresholds[k].warn;
              T[k].danger = data.thresholds[k].danger;
              if (data.thresholds[k].label) T[k].label = data.thresholds[k].label;
              if (data.thresholds[k].unit) T[k].unit = data.thresholds[k].unit;
            }
          });
          const d = {};
          Object.keys(T).forEach((k) => (d[k] = { warn: T[k].warn, danger: T[k].danger }));
          setDraft(d);
        }
      })
      .catch(() => {});
  }, [isReal]);

  async function save() {
    if (isReal) {
      setSaving(true);
      setError(null);
      try {
        const deviceId = sim._realDeviceId || "default";
        const thresholds = {};
        Object.keys(draft).forEach((k) => {
          thresholds[k] = { warn: Number(draft[k].warn), danger: Number(draft[k].danger) };
        });
        const res = await fetch('/api/thresholds', {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ device_id: deviceId, thresholds }),
        });
        const data = await res.json();
        if (data.ok) {
          Object.keys(draft).forEach((k) => { T[k].warn = Number(draft[k].warn); T[k].danger = Number(draft[k].danger); });
          setSavedAt(Date.now());
        } else {
          setError(data.error || "保存失败");
        }
      } catch (e) {
        setError(e.message);
      }
      setSaving(false);
    } else {
      Object.keys(draft).forEach((k) => { T[k].warn = Number(draft[k].warn); T[k].danger = Number(draft[k].danger); });
      setSavedAt(Date.now());
    }
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16, maxWidth: 880 }} data-screen-label="风险阈值">
      <Card title="告警阈值" sub={isReal ? "GET/PUT /api/thresholds · real" : "GET/PUT /api/thresholds · mock"}
        right={<div style={{ display: "flex", gap: 8, alignItems: "center" }}>
          {error && <span style={{ color: "var(--danger)", fontSize: 11 }}>{error}</span>}
          {savedAt && !dirty && <span className="t3 mono" style={{ fontSize: 11 }}>已保存 ✓</span>}
          <button className="btn" disabled={!dirty || saving} style={{ opacity: dirty ? 1 : 0.45 }} onClick={() => { const d = {}; Object.keys(T).forEach((k) => (d[k] = { warn: T[k].warn, danger: T[k].danger })); setDraft(d); }}>还原</button>
          <button className="btn primary" disabled={!dirty || saving} style={{ opacity: dirty ? 1 : 0.45 }} onClick={save}>{saving ? "保存中…" : "保存到云端"}</button>
        </div>}>
        <div className="thr-row" style={{ borderBottom: "1px solid var(--line)", paddingTop: 0 }}>
          <span style={{ fontSize: 11, color: "var(--text-3)", letterSpacing: "0.06em" }}>指标</span>
          <span style={{ fontSize: 11, color: "var(--text-3)", width: 96, textAlign: "right" }}>当前值</span>
          <span style={{ fontSize: 11, color: "var(--warn)", width: 76, textAlign: "right" }}>warn ≥</span>
          <span style={{ fontSize: 11, color: "var(--danger)", width: 76, textAlign: "right" }}>danger ≥</span>
        </div>
        {Object.keys(T).map((k) => {
          const t = T[k]; const v = cur[k];
          const lvl = v == null ? "off" : v >= Number(draft[k].danger) ? "danger" : v >= Number(draft[k].warn) ? "warn" : "ok";
          return (
            <div className="thr-row" key={k}>
              <div style={{ minWidth: 0 }}><div style={{ fontSize: 13, fontWeight: 500 }}>{t.label}</div><div className="mono t3" style={{ fontSize: 11 }}>{k} · {t.unit}</div></div>
              <span className="num" style={{ width: 96, textAlign: "right", fontSize: 14, fontWeight: 600, color: riskColor(lvl) }}>{v == null ? "—" : v}</span>
              <input className="input" type="number" step="0.5" value={draft[k].warn} onChange={(e) => setDraft({ ...draft, [k]: { ...draft[k], warn: e.target.value } })} />
              <input className="input" type="number" step="0.5" value={draft[k].danger} onChange={(e) => setDraft({ ...draft, [k]: { ...draft[k], danger: e.target.value } })} />
            </div>
          );
        })}
      </Card>
      <Card title="生效机制">
        <ol style={{ margin: 0, padding: "0 0 0 18px", fontSize: 12.5, color: "var(--text-2)", lineHeight: 1.9 }}>
          <li>云端阈值影响后端告警生成与前端展示。</li>
          <li>warn 级仅产生告警记录与事件流通知；danger 级同时点亮总览红色横幅。</li>
          <li>设备端本地安全闭环使用设备端内置阈值，云端配置不影响本地兜底。</li>
        </ol>
      </Card>
    </div>
  );
}
