import React from 'react'
import { Card, Tag } from '../components/Widgets'
import { TimeChart, BinaryStrip } from '../components/Charts'

export default function PageSensors({ sim }) {
  const [win, setWin] = React.useState(600);
  const T = sim.thresholds;
  const off = sim.scenario === "offline";

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16 }} data-screen-label="传感器">
      <div style={{ display: "flex", alignItems: "center", gap: 12, flexWrap: "wrap" }}>
        <span className="t3" style={{ fontSize: 12 }}>采样 1 Hz · 批量上报 30 s · 时间窗</span>
        <div className="seg">
          {[{ s: 120, l: "2 min" }, { s: 300, l: "5 min" }, { s: 600, l: "10 min" }].map((o) => (
            <button key={o.s} className={win === o.s ? "on" : ""} onClick={() => setWin(o.s)}>{o.l}</button>
          ))}
        </div>
        {off && <Tag level="danger">离线 — 显示最后快照</Tag>}
      </div>
      <div className="grid" style={{ gridTemplateColumns: "repeat(auto-fit, minmax(420px, 1fr))" }}>
        <Card title="MPU-6500 · 震动分数" sub="I²C · 100Hz 采样 → 1Hz 聚合"
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("vib")}</span>}>
          <TimeChart data={sim.getSeries("vib")} window={win} warn={T["mpu6500.vibration_score"].warn} danger={T["mpu6500.vibration_score"].danger} yMin={0} yMax={10} height={140} color="var(--accent)" />
        </Card>
        <Card title="MPU-6500 · 加速度三轴" sub="g">
          <TimeChart window={win} height={140} area={false} series={[
            { name: "ax", color: "#57c1d5", data: sim.getSeries("ax") },
            { name: "ay", color: "#5d9cf5", data: sim.getSeries("ay") },
            { name: "az", color: "#a18ef5", data: sim.getSeries("az") },
          ]} />
        </Card>
        <Card title="DHT11 · 温度" sub="°C · 外接 MCU 预留 · mock 数据"
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("temp")}°C</span>}>
          <TimeChart data={sim.getSeries("temp")} window={win} height={140} color="#5d9cf5" />
        </Card>
        <Card title="DHT11 · 湿度" sub="%RH · 外接 MCU 预留 · mock 数据"
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("hum")}%</span>}>
          <TimeChart data={sim.getSeries("hum")} window={win} height={140} color="#41bd80" />
        </Card>
        <Card title="安全传感器 · 二值信号" sub="PIR / 火焰 / MQ-2 烟雾">
          <div style={{ paddingTop: 6 }}>
            <BinaryStrip window={win} height={20} rows={[
              { label: "PIR", data: sim.getSeries("pir"), color: "#5d9cf5" },
              { label: "FLAME", data: sim.getSeries("flame"), color: "#e5484d" },
              { label: "MQ-2", data: sim.getSeries("mq2"), color: "#e0a23e" },
            ]} />
          </div>
          <p className="t3" style={{ margin: "12px 0 0", fontSize: 11.5, lineHeight: 1.6 }}>
            本地安全动作由设备端独立执行，后端与前端只负责展示、记录和通知。
          </p>
        </Card>
      </div>
    </div>
  );
}
