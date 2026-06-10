import React from 'react'
import { Card, Pill, Tag, Empty, VideoSurface, systemStatus, riskLevel, riskColor, timeAgo } from '../components/Widgets'
import { TimeChart, BinaryStrip } from '../components/Charts'

export default function PageLive({ sim }) {
  const T = sim.thresholds;
  const risk = sim.latest("risk");
  const offline = sim.scenario === "offline";
  const ai = sim.ai.find((o) => o.ok);

  return (
    <div className="grid" style={{ gridTemplateColumns: "minmax(0, 8fr) minmax(0, 4fr)" }} data-screen-label="实时巡检">
      <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
        <Card title="CAM-01 · 巡检点位 A" sub="MJPEG over HTTP · /api/video/stream" flush
          right={<Pill level={offline ? "danger" : "ok"} blink={!offline}>{offline ? "断流" : "直播中"}</Pill>}>
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
          {ai ? (
            <div style={{ display: "flex", flexDirection: "column", gap: 9 }}>
              <div style={{ display: "flex", gap: 7, flexWrap: "wrap", alignItems: "center" }}>
                {ai.status === "mock" ? <Tag level="degraded">mock</Tag> : <Tag level={ai.risk_hint >= 7 ? "danger" : ai.risk_hint >= 4 ? "warn" : "ok"}>{ai.risk_hint}/10</Tag>}
                <span className="mono t3" style={{ fontSize: 11 }}>{timeAgo(ai.timestamp)}</span>
              </div>
              <p style={{ margin: 0, fontSize: 13, lineHeight: 1.7, color: "var(--text-2)" }}>{ai.full_text || ai.summary}</p>
            </div>
          ) : <Empty>暂无观察</Empty>}
        </Card>
        <Card title="安全传感器">
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
