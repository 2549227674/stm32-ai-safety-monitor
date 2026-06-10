/* ============================================================
   charts.jsx — 轻量 SVG 图表（无外部依赖）
   ============================================================ */
import React, { useState, useEffect, useRef, useMemo } from 'react'

function useSize() {
  const ref = useRef(null);
  const [w, setW] = useState(0);
  useEffect(() => {
    if (!ref.current) return;
    const ro = new ResizeObserver((es) => {
      const cw = es[0].contentRect.width;
      setW((p) => (Math.abs(p - cw) > 1 ? cw : p));
    });
    ro.observe(ref.current);
    return () => ro.disconnect();
  }, []);
  return [ref, w];
}

function fmtClock(t) {
  const d = new Date(t * 1000);
  return String(d.getHours()).padStart(2, "0") + ":" + String(d.getMinutes()).padStart(2, "0");
}

const CHART_C = {
  grid: "#1d222b",
  axis: "#646d7c",
  warn: "#e0a23e",
  danger: "#e5484d",
};

/* data: [{t,v}] 或多序列 [{name,color,data}] */
function TimeChart({
  data, series, height = 130, unit = "", warn, danger,
  yMin, yMax, color = "var(--accent)", area = true, fmt = (v) => v,
  window: winSec = 600,
}) {
  const [ref, w] = useSize();
  const H = height, padL = 36, padR = 8, padT = 8, padB = 18;

  const allSeries = series || [{ name: "", color, data: data || [] }];
  const tNow = Math.floor(Date.now() / 1000);
  const t0 = tNow - winSec;

  let lo = Infinity, hi = -Infinity;
  allSeries.forEach((s) => s.data.forEach((p) => { if (p.t >= t0) { if (p.v < lo) lo = p.v; if (p.v > hi) hi = p.v; } }));
  if (!isFinite(lo)) { lo = 0; hi = 1; }
  if (warn != null) hi = Math.max(hi, warn);
  if (danger != null) hi = Math.max(hi, danger);
  if (yMin != null) lo = yMin;
  if (yMax != null) hi = yMax;
  if (hi === lo) hi = lo + 1;
  const span = hi - lo;
  lo -= span * 0.08; hi += span * 0.1;

  const iw = Math.max(10, w - padL - padR);
  const ih = H - padT - padB;
  const X = (t) => padL + ((t - t0) / winSec) * iw;
  const Y = (v) => padT + (1 - (v - lo) / (hi - lo)) * ih;

  const paths = allSeries.map((s) => {
    const pts = s.data.filter((p) => p.t >= t0);
    if (!pts.length) return { line: "", area: "", color: s.color, name: s.name, last: null };
    const line = pts.map((p, i) => (i ? "L" : "M") + X(p.t).toFixed(1) + " " + Y(p.v).toFixed(1)).join(" ");
    const ar = line + ` L${X(pts[pts.length - 1].t).toFixed(1)} ${padT + ih} L${X(pts[0].t).toFixed(1)} ${padT + ih} Z`;
    return { line, area: ar, color: s.color, name: s.name, last: pts[pts.length - 1] };
  });

  // y ticks: 3 lines
  const yTicks = [0.0, 0.5, 1.0].map((f) => lo + (hi - lo) * (0.08 + 0.82 * f));
  // x ticks: every winSec/4
  const xTicks = [1, 2, 3].map((i) => t0 + (winSec * i) / 4);
  const single = allSeries.length === 1;
  const gid = useMemo(() => "g" + Math.random().toString(36).slice(2, 8), []);

  return (
    <div ref={ref} style={{ width: "100%", position: "relative" }}>
      {w > 0 && (
        <svg width={w} height={H} style={{ display: "block" }}>
          <defs>
            <linearGradient id={gid} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor={paths[0].color} stopOpacity="0.22" />
              <stop offset="100%" stopColor={paths[0].color} stopOpacity="0.01" />
            </linearGradient>
          </defs>
          {yTicks.map((v, i) => (
            <g key={i}>
              <line x1={padL} x2={w - padR} y1={Y(v)} y2={Y(v)} stroke={CHART_C.grid} strokeWidth="1" />
              <text x={padL - 6} y={Y(v) + 3.5} fill={CHART_C.axis} fontSize="9.5" fontFamily="var(--mono)" textAnchor="end">{fmt(Math.round(v * 10) / 10)}</text>
            </g>
          ))}
          {xTicks.map((t, i) => (
            <text key={i} x={X(t)} y={H - 5} fill={CHART_C.axis} fontSize="9.5" fontFamily="var(--mono)" textAnchor="middle">{fmtClock(t)}</text>
          ))}
          {warn != null && (
            <g>
              <line x1={padL} x2={w - padR} y1={Y(warn)} y2={Y(warn)} stroke={CHART_C.warn} strokeWidth="1" strokeDasharray="5 4" opacity="0.75" />
              <text x={w - padR - 2} y={Y(warn) - 3} fill={CHART_C.warn} fontSize="9" fontFamily="var(--mono)" textAnchor="end">warn {fmt(warn)}</text>
            </g>
          )}
          {danger != null && (
            <g>
              <line x1={padL} x2={w - padR} y1={Y(danger)} y2={Y(danger)} stroke={CHART_C.danger} strokeWidth="1" strokeDasharray="5 4" opacity="0.85" />
              <text x={w - padR - 2} y={Y(danger) - 3} fill={CHART_C.danger} fontSize="9" fontFamily="var(--mono)" textAnchor="end">danger {fmt(danger)}</text>
            </g>
          )}
          {single && area && paths[0].area && <path d={paths[0].area} fill={`url(#${gid})`} />}
          {paths.map((p, i) => p.line && <path key={i} d={p.line} fill="none" stroke={p.color} strokeWidth="1.6" strokeLinejoin="round" />)}
          {paths.map((p, i) =>
            p.last ? <circle key={"c" + i} cx={X(p.last.t)} cy={Y(p.last.v)} r="2.6" fill={p.color} /> : null
          )}
        </svg>
      )}
      {!single && (
        <div style={{ display: "flex", gap: 12, flexWrap: "wrap", padding: "2px 0 0 36px" }}>
          {allSeries.map((s, i) => (
            <span key={i} style={{ fontSize: 10.5, fontFamily: "var(--mono)", color: "var(--text-3)" }}>
              <span style={{ display: "inline-block", width: 9, height: 3, background: s.color, borderRadius: 2, marginRight: 5, verticalAlign: "2px" }}></span>
              {s.name}
            </span>
          ))}
        </div>
      )}
      {unit && <span style={{ position: "absolute", top: 2, right: 6, fontSize: 10, fontFamily: "var(--mono)", color: "var(--text-3)" }}>{unit}</span>}
    </div>
  );
}

function Spark({ data, color = "var(--accent)", width = 110, height = 30, window: winSec = 600 }) {
  const tNow = Math.floor(Date.now() / 1000), t0 = tNow - winSec;
  const pts = (data || []).filter((p) => p.t >= t0);
  if (pts.length < 2) return <svg width={width} height={height}></svg>;
  let lo = Infinity, hi = -Infinity;
  pts.forEach((p) => { if (p.v < lo) lo = p.v; if (p.v > hi) hi = p.v; });
  if (hi === lo) hi = lo + 1;
  const X = (t) => ((t - t0) / winSec) * width;
  const Y = (v) => 2 + (1 - (v - lo) / (hi - lo)) * (height - 4);
  const d = pts.map((p, i) => (i ? "L" : "M") + X(p.t).toFixed(1) + " " + Y(p.v).toFixed(1)).join(" ");
  return (
    <svg width={width} height={height} style={{ display: "block" }}>
      <path d={d} fill="none" stroke={color} strokeWidth="1.4" />
      <circle cx={X(pts[pts.length - 1].t)} cy={Y(pts[pts.length - 1].v)} r="2" fill={color} />
    </svg>
  );
}

/* 二值传感器时间带 */
function BinaryStrip({ rows, window: winSec = 600, height = 16 }) {
  const [ref, w] = useSize();
  const tNow = Math.floor(Date.now() / 1000), t0 = tNow - winSec;
  return (
    <div ref={ref} style={{ width: "100%", display: "flex", flexDirection: "column", gap: 8 }}>
      {rows.map((row, ri) => {
        const pts = (row.data || []).filter((p) => p.t >= t0);
        const segs = [];
        let start = null;
        pts.forEach((p, i) => {
          if (p.v && start == null) start = p.t;
          if ((!p.v || i === pts.length - 1) && start != null) {
            segs.push([start, p.v ? p.t : pts[Math.max(0, i - 1)].t]);
            start = null;
          }
        });
        const X = (t) => ((t - t0) / winSec) * Math.max(1, w - 70);
        return (
          <div key={ri} style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <span style={{ width: 60, flex: "none", fontSize: 11, fontFamily: "var(--mono)", color: "var(--text-3)" }}>{row.label}</span>
            <div style={{ position: "relative", flex: 1, height, background: "var(--inset)", border: "1px solid var(--line)", borderRadius: 4, overflow: "hidden" }}>
              {w > 0 && segs.map(([a, b], i) => (
                <div key={i} style={{
                  position: "absolute", top: 0, bottom: 0,
                  left: X(a), width: Math.max(2, X(b) - X(a)),
                  background: row.color, opacity: 0.85,
                }}></div>
              ))}
            </div>
            <span className="tag" style={{ width: 34, justifyContent: "center", color: pts.length && pts[pts.length - 1].v ? row.color : undefined }}>
              {pts.length && pts[pts.length - 1].v ? "HIGH" : "LOW"}
            </span>
          </div>
        );
      })}
    </div>
  );
}

/* AI 推理耗时瀑布 */
function TimingBar({ metrics }) {
  if (!metrics) return null;
  const segs = [
    { k: "imgenc_ms", label: "image encoder", color: "#57c1d5" },
    { k: "llm_prefill_ms", label: "LLM prefill", color: "#5d9cf5" },
    { k: "decode_ms", label: "decode", color: "#a18ef5" },
  ].filter((s) => metrics[s.k] > 0);
  const total = metrics.total_ms || segs.reduce((a, s) => a + metrics[s.k], 0);
  if (!segs.length || !total) return null;
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
      <div className="tbar">
        {segs.map((s) => (
          <div key={s.k} title={`${s.label} ${metrics[s.k]}ms`} style={{ width: (metrics[s.k] / total) * 100 + "%", background: s.color, opacity: 0.8 }}></div>
        ))}
        <div style={{ flex: 1, background: "var(--raised)" }}></div>
      </div>
      <div className="tlegend">
        {segs.map((s) => (
          <span key={s.k}><span className="sw" style={{ background: s.color }}></span>{s.label} {(metrics[s.k] / 1000).toFixed(2)}s</span>
        ))}
        <span style={{ marginLeft: "auto", color: "var(--text-2)" }}>total {(total / 1000).toFixed(2)}s</span>
      </div>
    </div>
  );
}

export { useSize, TimeChart, Spark, BinaryStrip, TimingBar, fmtClock };
