import React from 'react'
import { Card, Tag } from '../components/Widgets'
import { TimeChart, BinaryStrip } from '../components/Charts'

export default function PageSensors({ sim }) {
  const [win, setWin] = React.useState(600);
  const T = sim.thresholds;
  const off = sim.scenario === "offline";
  const isReal = sim.mode === "real";
  const src = isReal ? (sim.sensorsSource || {}) : {};

  // Safety sensor subtitle: shows data source when in real mode
  const safetySub = (() => {
    if (!isReal) return "PIR / 火焰 / MQ-2";
    const age = sim.realtimeAge();
    if (age === Infinity) return "PIR / 火焰 / MQ-2 · safetyd 未运行/状态文件缺失";
    const source = src.safety;
    const sourceLabel = source === "safetyd_status_file" ? "safetyd" : source === "fallback_mock" ? "fallback mock" : "unknown source";
    if (age > 3) return `PIR / 火焰 / MQ-2 · 数据滞后 ${Math.round(age)}s · ${sourceLabel}`;
    return `PIR / 火焰 / MQ-2 · 实时 · ${sourceLabel}`;
  })();

  // MPU subtitle
  const mpuSub = (() => {
    if (!isReal) return "I²C · 100Hz 采样 → 1Hz 聚合";
    const mpuSrc = src.mpu6500;
    if (mpuSrc === "real_i2c") return "I²C · /dev/i2c-1 · 0x68 · 真实 MPU-6500";
    if (mpuSrc === "mock" || mpuSrc === "unknown" || !mpuSrc) return "mock 数据 · 未接真实 MPU-6500";
    return "I²C · 100Hz 采样 → 1Hz 聚合";
  })();

  // Env sensor subtitle: always show mock for env
  const envTempSub = (() => {
    if (!isReal) return "°C · 外接 MCU 预留 · mock 数据";
    const envSrc = src.env;
    if (envSrc === "mock" || envSrc === "unknown" || !envSrc) return "°C · 外接 MCU 预留 · mock 数据";
    return "°C · env sensor from device-agent";
  })();

  const envHumSub = (() => {
    if (!isReal) return "%RH · 外接 MCU 预留 · mock 数据";
    const envSrc = src.env;
    if (envSrc === "mock" || envSrc === "unknown" || !envSrc) return "%RH · 外接 MCU 预留 · mock 数据";
    return "%RH · env sensor from device-agent";
  })();

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16 }} data-screen-label="传感器">
      <div style={{ display: "flex", alignItems: "center", gap: 12, flexWrap: "wrap" }}>
        <span className="t3" style={{ fontSize: 12 }}>{'采样 1 Hz · 实时 SSE · 历史 30s 补全 · 时间窗'}</span>
        <div className="seg">
          {[{ s: 120, l: "2 min" }, { s: 300, l: "5 min" }, { s: 600, l: "10 min" }].map((o) => (
            <button key={o.s} className={win === o.s ? "on" : ""} onClick={() => setWin(o.s)}>{o.l}</button>
          ))}
        </div>
        {off && <Tag level="danger">{'离线 — 显示最后快照'}</Tag>}
        {isReal && src.safety === "fallback_mock" && <Tag level="warn">{'safetyd 未运行'}</Tag>}
      </div>
      <div className="grid" style={{ gridTemplateColumns: "repeat(auto-fit, minmax(420px, 1fr))" }}>
        <Card title="MPU-6500 · 震动分数" sub={mpuSub}
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("vib")}</span>}>
          <TimeChart data={sim.getSeries("vib")} window={win} warn={T["mpu6500.vibration_score"].warn} danger={T["mpu6500.vibration_score"].danger} yMin={0} yMax={10} height={140} color="var(--accent)" />
        </Card>
        <Card title="MPU-6500 · 加速度三轴" sub={isReal ? (src.mpu6500 === "real_i2c" ? "g · 真实 MPU-6500" : (src.mpu6500 === "mock" || src.mpu6500 === "unknown" || !src.mpu6500) ? "mock 数据 · 未接入真实 MPU" : "g") : "g"}>
          <TimeChart window={win} height={140} area={false} series={[
            { name: "ax", color: "#57c1d5", data: sim.getSeries("ax") },
            { name: "ay", color: "#5d9cf5", data: sim.getSeries("ay") },
            { name: "az", color: "#a18ef5", data: sim.getSeries("az") },
          ]} />
        </Card>
        <Card title="DHT11 · 温度" sub={envTempSub}
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("temp")}{'°C'}</span>}>
          <TimeChart data={sim.getSeries("temp")} window={win} height={140} color="#5d9cf5" />
        </Card>
        <Card title="DHT11 · 湿度" sub={envHumSub}
          right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{sim.latest("hum")}{'%'}</span>}>
          <TimeChart data={sim.getSeries("hum")} window={win} height={140} color="#41bd80" />
        </Card>
        <Card title={'安全传感器 · 二值信号'} sub={safetySub}>
          <div style={{ paddingTop: 6 }}>
            <BinaryStrip window={win} height={20} rows={[
              { label: "PIR", data: sim.getSeries("pir"), color: "#5d9cf5" },
              { label: "FLAME", data: sim.getSeries("flame"), color: "#e5484d" },
              { label: "MQ-2", data: sim.getSeries("mq2"), color: "#e0a23e" },
            ]} />
          </div>
          <p className="t3" style={{ margin: "12px 0 0", fontSize: 11.5, lineHeight: 1.6 }}>
            {'本地安全动作由设备端独立执行，后端与前端只负责展示、记录和通知。'}
          </p>
        </Card>
      </div>
    </div>
  );
}
