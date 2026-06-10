import React from 'react'
import { Card, Tag, Empty, fmtTime, LEVEL_LABEL } from '../components/Widgets'

function NotifStatusTag({ status }) {
  if (status === "sent") return <Tag level="ok">sent</Tag>;
  if (status === "failed") return <Tag level="danger">failed</Tag>;
  if (status === "skipped") return <Tag level="warn">skipped</Tag>;
  return <Tag>—</Tag>;
}

export default function PageAlerts({ sim }) {
  const [tab, setTab] = React.useState("alerts");
  const [lvl, setLvl] = React.useState("all");
  const list = tab === "alerts" ? sim.alerts : sim.events;
  const filtered = lvl === "all" ? list : list.filter((e) => e.level === lvl);

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16, maxWidth: 980 }} data-screen-label="事件告警">
      <div style={{ display: "flex", alignItems: "center", gap: 12, flexWrap: "wrap" }}>
        <div className="seg">
          <button className={tab === "alerts" ? "on" : ""} onClick={() => setTab("alerts")}>告警 ({sim.alerts.length})</button>
          <button className={tab === "events" ? "on" : ""} onClick={() => setTab("events")}>全部事件 ({sim.events.length})</button>
        </div>
        <div className="seg">
          {[["all", "全部"], ["danger", "ALERT"], ["warn", "WARN"], ["info", "INFO"]].map(([v, l]) => (
            <button key={v} className={lvl === v ? "on" : ""} onClick={() => setLvl(v)}>{l}</button>
          ))}
        </div>
        <span className="t3 mono" style={{ fontSize: 11, marginLeft: "auto" }}>SSE /api/stream/events · 实时</span>
      </div>
      <Card flush>
        {filtered.length ? (
          <table className="table">
            <thead><tr><th style={{ width: 96 }}>时间</th><th style={{ width: 72 }}>级别</th><th style={{ width: 90 }}>来源</th><th>内容</th>{tab === "alerts" && <th style={{ width: 86 }}>邮件</th>}</tr></thead>
            <tbody>
              {filtered.slice(0, 60).map((e) => (
                <tr key={e.id}>
                  <td className="mono" style={{ fontSize: 11.5, color: "var(--text-3)", whiteSpace: "nowrap" }}>{fmtTime(e.ts)}</td>
                  <td><Tag level={e.level === "danger" ? "danger" : e.level === "warn" ? "warn" : "info"}>{LEVEL_LABEL[e.level]}</Tag></td>
                  <td className="mono" style={{ fontSize: 11.5 }}>{e.source || e.metric || "—"}</td>
                  <td style={{ color: e.level === "danger" ? "#efb6b8" : undefined }}>{e.msg}</td>
                  {tab === "alerts" && <td>{e.email ? <NotifStatusTag status={e.email.status} /> : <span className="t3 mono" style={{ fontSize: 11 }}>—</span>}</td>}
                </tr>
              ))}
            </tbody>
          </table>
        ) : <Empty icon="✓">{tab === "alerts" ? "当前没有告警 — 系统运行正常" : "暂无事件"}</Empty>}
      </Card>
      <p className="t3" style={{ margin: 0, fontSize: 11.5 }}>告警由云端按阈值规则生成，仅用于通知与记录；danger 级告警同时触发邮件通知（见「通知设置」）。执行器动作始终由设备端本地决策。</p>
    </div>
  );
}
