import React from 'react'
import { Card, Pill, Tag, Empty, VideoSurface, systemStatus, riskLevel, riskColor, timeAgo, fmtTime, LEVEL_LABEL } from '../components/Widgets'
import { TimeChart, Spark, BinaryStrip } from '../components/Charts'
import IsoScene from '../components/Scene'

export default function PageOverview({ sim, nav }) {
  const st = systemStatus(sim);
  const risk = sim.latest("risk");
  const dev = sim.device || {};
  const isReal = sim.mode === "real";
  const ai = sim.ai.find((o) => o.ok);
  const offline = isReal ? !dev.online : sim.scenario === "offline";
  const deg = isReal ? (dev.ai && !dev.ai.ok && !dev.ai.model_ready) : sim.scenario === "degraded";
  const T = sim.thresholds;
  const dangerAlerts = sim.alerts.filter((a) => a.level === "danger").slice(0, 1);

  // Real mode: check if AI observation is stale
  const now = Math.floor(Date.now() / 1000);
  const aiTs = ai ? (typeof ai.timestamp === "number" ? ai.timestamp : new Date(ai.timestamp).getTime() / 1000) : 0;
  const stale = isReal && ai && (now - aiTs) > 90;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16 }} data-screen-label="总览">
      {st.level === "danger" && (
        <div className="banner danger">
          <span className="blink" style={{ fontSize: 16 }}>●</span>
          <span style={{ flex: 1 }}><b>告警进行中</b> — {dangerAlerts[0] ? dangerAlerts[0].msg : "风险分数超过 danger 阈值"}。设备端本地告警闭环已独立执行。</span>
          <button className="btn" onClick={() => nav("alerts")}>查看告警</button>
        </div>
      )}
      {st.level === "degraded" && (
        <div className="banner degraded">
          <span style={{ fontSize: 16 }}>◐</span>
          <span style={{ flex: 1 }}><b>降级运行</b> — Qwen3-VL 推理服务不可达，AI 观察已切换 mock 占位输出；传感器遥测与本地安全闭环不受影响。</span>
          <button className="btn" onClick={() => nav("ai")}>查看 AI 服务</button>
        </div>
      )}
      {offline && (
        <div className="banner off">
          <span style={{ fontSize: 16 }}>◌</span>
          <span style={{ flex: 1 }}><b style={{ color: "var(--text)" }}>设备离线</b> — 超过 15s 未收到心跳，显示为最后一次上报的数据快照。设备端不依赖云端，本地安全闭环持续运行。</span>
          <button className="btn" onClick={() => nav("device")}>设备详情</button>
        </div>
      )}
      <Card flush title="设施全貌" sub="数字孪生 · 上帝视角"
        right={<div style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <span className="t3 mono" style={{ fontSize: 11 }}>{offline ? "快照" : "实时"}</span>
          <button className="btn ghost" onClick={() => nav("live")}>进入巡检 →</button>
        </div>}>
        <IsoScene sim={sim} />
      </Card>
      <div className="status-strip">
        <div className="status-cell"><span className="lbl">系统状态</span><span className="val"><Pill level={st.pill} blink={st.blink}>{st.label}</Pill></span></div>
        <div className="status-cell"><span className="lbl">风险分数</span><span className="val num" style={{ color: riskColor(riskLevel(risk)), fontSize: 17 }}>{offline ? "—" : risk != null ? risk.toFixed(1) : "—"}<span className="sm">/10</span></span></div>
        <div className="status-cell"><span className="lbl">AI 模型</span><span className="val" style={{ fontSize: 12.5 }}>{offline ? <Tag>未知</Tag> : deg ? <Tag level="degraded">mock 降级</Tag> : <Tag level="ok">qwen3-vl-2b</Tag>}<span className="sm">{!offline && !deg ? "RKNN·NPU" : ""}</span></span></div>
        <div className="status-cell"><span className="lbl">最近 AI 观察</span><span className="val num" style={{ fontSize: 13 }}>{ai && !stale ? timeAgo(ai.timestamp) : isReal ? "—" : "—"}</span></div>
        <div className="status-cell"><span className="lbl">设备心跳</span><span className="val num" style={{ fontSize: 13, color: offline ? "var(--danger)" : undefined }}>{offline ? "丢失 " + timeAgo(dev.last_seen_at) : timeAgo(dev.last_seen_at)}</span></div>
        <div className="status-cell"><span className="lbl">CPU 温度</span><span className="val num" style={{ fontSize: 13, color: sim.latest("cpu_temp") >= T.cpu_temp_c.warn ? "var(--warn)" : undefined }}>{sim.latest("cpu_temp")}°C</span></div>
      </div>
      <div className="grid" style={{ gridTemplateColumns: "minmax(0, 7fr) minmax(0, 5fr)" }}>
        <Card title="实时画面" sub="CAM-01 · /api/video/stream" flush
          right={<button className="btn ghost" onClick={() => nav("live")}>进入巡检 →</button>}>
          <div style={{ padding: "0 16px 16px" }}><VideoSurface sim={sim} /></div>
        </Card>
        <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
          <Card title="最近 AI 观察" sub="GET /api/ai/observations/latest"
            right={<button className="btn ghost" onClick={() => nav("ai")}>全部 →</button>}>
            {(() => {
              const showAi = ai && !stale;
              const isMock = !isReal || (ai && ai.status === "mock");

              if (!showAi) {
                return <Empty>{isReal ? "暂无有效 AI 观察 · 等待设备上传" : "等待首次 AI 观察…"}</Empty>;
              }
              return (
                <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
                  <div style={{ display: "flex", gap: 8, alignItems: "center", flexWrap: "wrap" }}>
                    {isMock ? <Tag level="degraded">mock</Tag> : <Tag level={ai.risk_hint >= 7 ? "danger" : ai.risk_hint >= 4 ? "warn" : "ok"}>risk_hint {ai.risk_hint}/10</Tag>}
                    {(ai.labels || []).map((l) => <Tag key={l}>{l}</Tag>)}
                    <span className="mono t3" style={{ fontSize: 11, marginLeft: "auto" }}>{fmtTime(ai.timestamp)}</span>
                  </div>
                  <p style={{ margin: 0, fontSize: 13.5, lineHeight: 1.65, color: "var(--text)" }}>
                    {isMock ? <span style={{ color: "var(--text-3)" }}>[mock] </span> : null}{ai.summary}
                  </p>
                </div>
              );
            })()}
          </Card>
          <Card title="风险趋势" sub="10 min">
            <TimeChart data={sim.getSeries("risk")} warn={T.risk_score.warn} danger={T.risk_score.danger} yMin={0} yMax={10} height={120} color={riskColor(riskLevel(risk))} />
          </Card>
        </div>
      </div>
      <div className="grid" style={{ gridTemplateColumns: "minmax(0, 7fr) minmax(0, 5fr)" }}>
        <Card title="传感器速览" sub="telemetry · 1s 采样 / 30s 批量上报"
          right={<button className="btn ghost" onClick={() => nav("sensors")}>详情 →</button>}>
          <div className="grid" style={{ gridTemplateColumns: "repeat(auto-fit, minmax(165px, 1fr))", gap: 10 }}>
            {[
              { k: "vib", label: "震动分数", unit: "", color: "var(--accent)", warnAt: sim.thresholds["mpu6500.vibration_score"].warn },
              { k: "cpu_temp", label: "CPU 温度", unit: "°C", color: "#e0a23e" },
              { k: "temp", label: "环境温度 (mock)", unit: "°C", color: "#5d9cf5" },
              { k: "hum", label: "湿度 (mock)", unit: "%", color: "#41bd80" },
            ].map((m) => {
              const v = sim.latest(m.k);
              const hot = m.warnAt != null && v >= m.warnAt;
              return (
                <div key={m.k} style={{ border: "1px solid var(--line)", borderRadius: "var(--r-md)", padding: "10px 12px", background: "var(--raised)", display: "flex", flexDirection: "column", gap: 6, minWidth: 0 }}>
                  <span style={{ fontSize: 11, color: "var(--text-3)" }}>{m.label}</span>
                  <span className="num" style={{ fontSize: 20, fontWeight: 600, color: hot ? "var(--warn)" : "var(--text)" }}>
                    {offline ? "—" : v}<span style={{ fontSize: 11, color: "var(--text-3)", fontWeight: 400 }}> {m.unit}</span>
                  </span>
                  <Spark data={sim.getSeries(m.k)} color={m.color} width={140} height={26} />
                </div>
              );
            })}
          </div>
          <div style={{ marginTop: 12 }}>
            <BinaryStrip rows={[
              { label: "PIR", data: sim.getSeries("pir"), color: "#5d9cf5" },
              { label: "FLAME", data: sim.getSeries("flame"), color: "#e5484d" },
              { label: "MQ-2", data: sim.getSeries("mq2"), color: "#e0a23e" },
            ]} />
          </div>
        </Card>
        <Card title="事件流" sub="GET /api/stream/events (SSE)" flush
          right={<button className="btn ghost" onClick={() => nav("alerts")}>全部 →</button>}>
          <div className="feed" style={{ maxHeight: 318, overflowY: "auto" }}>
            {sim.events.slice(0, 14).map((e) => (
              <div key={e.id} className={"feed-item lvl-" + e.level}>
                <span className="ts">{fmtTime(e.ts)}</span>
                <Tag level={e.level === "danger" ? "danger" : e.level === "warn" ? "warn" : "info"}>{LEVEL_LABEL[e.level]}</Tag>
                {e.source === "patrol" && <Tag level="info">巡检</Tag>}
                <span className="msg">{e.source === "patrol" && e.patrol ? `舵机巡检 · ${e.patrol.angles?.map(a => a + "°").join(" / ") || ""} · 最高风险 ${e.patrol.max_risk_hint ?? "?"}` : e.msg}</span>
              </div>
            ))}
          </div>
        </Card>
      </div>
    </div>
  );
}
