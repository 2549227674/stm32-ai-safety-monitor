"""Collect system metrics from OPi5."""

import os


def collect_metrics():
    m = {}
    m["cpu_temp_c"] = _read_thermal()
    mem = _read_meminfo()
    m["mem_used_mb"] = mem.get("used_mb")
    m["mem_total_mb"] = mem.get("total_mb")
    load = _read_loadavg()
    m["cpu_load_1m"] = load
    m["disk_used_pct"] = _read_disk_pct()
    return m


def _read_thermal():
    path = os.environ.get("SOC_TEMP_PATH", "/sys/class/thermal/thermal_zone0/temp")
    try:
        with open(path) as f:
            return round(int(f.read().strip()) / 1000, 1)
    except Exception:
        return None


def _read_meminfo():
    info = {}
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                parts = line.split()
                if parts[0] == "MemTotal:":
                    info["total_mb"] = int(parts[1]) // 1024
                elif parts[0] == "MemAvailable:":
                    avail = int(parts[1]) // 1024
                    info["used_mb"] = info.get("total_mb", 0) - avail
        return info
    except Exception:
        return {}


def _read_loadavg():
    try:
        with open("/proc/loadavg") as f:
            return float(f.read().split()[0])
    except Exception:
        return None


def _read_disk_pct():
    try:
        st = os.statvfs("/")
        total = st.f_blocks * st.f_frsize
        free = st.f_bfree * st.f_frsize
        return round((1 - free / total) * 100, 1) if total else None
    except Exception:
        return None
