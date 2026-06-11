import React from 'react'
import { Card, Pill, Tag, Empty, VideoSurface, systemStatus, riskLevel, riskColor, timeAgo } from '../components/Widgets'
import { TimeChart, BinaryStrip } from '../components/Charts'

export default function PageLive({ sim }) {
  const T = sim.thresholds;
  const risk = sim.latest("risk");
  const isReal = sim.mode === "real";
  const dev = sim.device || {};
  const offline = isReal ? !dev.online : sim.scenario === "offline";
  const ai = sim.ai.find((o) => o.ok);

  // Real mode: derive video status from camera fields
  const cameraStatus = isReal ? sim.cameraStatus : (offline ? "offline" : "mock");
  const videoOnline = isReal ? sim.videoOnline : !offline;
  const isMockCam = cameraStatus === "mock";
  const isRealCam = cameraStatus === "online";

  // Live pill: real mode checks device + video, mock mode checks scenario
  const liveActive = isReal ? (!offline && videoOnline && (isRealCam || isMockCam)) : !offline;

  return (
    <div className="grid" style={{ gridTemplateColumns: "minmax(0, 8fr) minmax(0, 4fr)" }} data-screen-label="实时巡检">
      <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
        <Card title="CAM-01 · 巡检点位 A" sub="MJPEG over HTTP · /api/video/stream" flush
          right={<Pill level={liveActive ? "ok" : "danger"} blink={liveActive}>{liveActive ? (isMockCam ? "模拟中" : "直播中") : "断流"}</Pill>}>
          <div style={{ padding: "0 16px 16px" }}><VideoSurface sim={sim} /></div>
        </Card>
        <Card title="风险分数" sub="设备端 1Hz 计算 · 阈值见「风险阈值」页">
          <TimeChart data={sim.getSeries("risk")} warn={T.risk_score.warn} danger={T.risk_score.danger} yMin={0} yMax={10} height={150} color={riskColor(riskLevel(risk))} />
        </Card>
      </div>
      <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
        <Card title="当前判定">
          <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
            <div className="metric-lg">
              <span className="v" style={{ color: riskColor(riskLevel(risk)) }}>{offline ? "—" : risk != null ? risk.toFixed(1) : "—"}</span>
              <span className="u">/10 风险分数</span>
            </div>
            <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
              {(() => { const st = systemStatus(sim); return <Pill level={st.pill} blink={st.blink}>{st.label}</Pill>; })()}
              {sim.latest("flame") ? <Tag level="danger">火焰触发</Tag> : null}
              {sim.latest("mq2") ? <Tag level="warn">烟雾信号</Tag> : null}
              {sim.latest("pir") ? <Tag level="info">移动目标</Tag> : null}
            </div>
            <p className="t3" style={{ margin: 0, fontSize: 11.5, lineHeight: 1.6 }}>
              判定由设备端融合传感器与 AI risk_hint 得出；云端仅展示，不下发控制命令。
            </p>
          </div>
        </Card>
        <Card title="AI 巡检解读" sub="每 30s 一次">
          {(() => {
            const now = Math.floor(Date.now() / 1000);
            const stale = isReal && ai && (now - (typeof ai.timestamp === "number" ? ai.timestamp : new Date(ai.timestamp).getTime() / 1000)) > 90;
            const showAi = ai && !stale;
            const isMock = !isReal || (ai && ai.status === "mock");
            if (!showAi) {
              return <Empty>{isReal ? "暂无有效 AI 观察 · 等待设备上传" : "暂无观察"}</Empty>;
            }
            return (
              <div style={{ display: "flex", flexDirection: "column", gap: 9 }}>
                <div style={{ display: "flex", gap: 7, flexWrap: "wrap", alignItems: "center" }}>
                  {isMock ? <Tag level="degraded">mock</Tag> : <Tag level={ai.risk_hint >= 7 ? "danger" : ai.risk_hint >= 4 ? "warn" : "ok"}>{ai.risk_hint}/10</Tag>}
                  <span className="mono t3" style={{ fontSize: 11 }}>{timeAgo(ai.timestamp)}</span>
                </div>
                <p style={{ margin: 0, fontSize: 13, lineHeight: 1.7, color: "var(--text-2)" }}>
                  {isMock ? <span style={{ color: "var(--text-3)" }}>[mock] </span> : null}{ai.full_text || ai.summary}
                </p>
              </div>
            );
          })()}
        </Card>
        <Card title="安全传感器" sub={
          (() => {
            const age = sim.realtimeAge();
            if (!isReal) return "PIR / 火焰 / MQ-2";
            if (age === Infinity) return "PIR / 火焰 / MQ-2 · 等待连接";
            if (age > 3) return `PIR / 火焰 / MQ-2 · 数据滞后 ${Math.round(age)}s`;
            return "PIR / 火焰 / MQ-2 · 实时";
          })()
        }>
          <BinaryStrip rows={[
            { label: "PIR", data: sim.getSeries("pir"), color: "#5d9cf5" },
            { label: "FLAME", data: sim.getSeries("flame"), color: "#e5484d" },
            { label: "MQ-2", data: sim.getSeries("mq2"), color: "#e0a23e" },
          ]} />
        </Card>
      </div>
    </div>
  );
}
