#!/usr/bin/env python3
"""
DHT11 reader using lgpio (Python). Must run as root.
Usage: sudo python3 dht11_read.py <gpio> [count] [--diag]
Output: one JSON line per reading
"""

import json
import sys
import time
from datetime import datetime, timezone

try:
    import lgpio
except ImportError:
    print(json.dumps({"error": "lgpio not installed"}))
    sys.exit(1)


def read_dht11(gpio: int, diag: bool = False) -> dict:
    """Read DHT11 sensor. Returns dict with temp/humid or error."""
    base = (gpio // 32) * 32
    phys_chip_map = {0: 0, 32: 1, 64: 2, 96: 3, 128: 4, 160: 5}
    phys_chip = phys_chip_map.get(base)
    if phys_chip is None:
        return {"error": f"gpio {gpio} not in known chip range"}

    ln = gpio - base

    try:
        h = lgpio.gpiochip_open(phys_chip)
    except Exception as e:
        return {"error": f"gpiochip_open({phys_chip}) failed: {e}"}

    try:
        # === Diagnostic: check idle state ===
        lgpio.gpio_claim_input(h, ln)
        idle = lgpio.gpio_read(h, ln)
        if diag:
            print(f"[diag] idle state: {idle}", file=sys.stderr)
        if idle == 0:
            # Line stuck low — likely missing pull-up
            lgpio.gpiochip_close(h)
            return {
                "error": "DATA line stuck LOW before start signal. "
                         "Check: 1) 4.7k-10k pull-up to 3.3V on DATA, "
                         "2) DHT11 VCC connected to 3.3V, "
                         "3) GND connected, "
                         "4) DATA on correct GPIO pin"
            }

        # === Start signal: pull low for >= 18ms ===
        lgpio.gpio_free(h, ln)
        lgpio.gpio_claim_output(h, ln, 0)
        time.sleep(0.020)  # 20ms

        # === Release: switch to input ===
        lgpio.gpio_free(h, ln)
        lgpio.gpio_claim_input(h, ln)

        # === Wait for DHT11 response: LOW for ~80µs ===
        t0 = time.monotonic()
        while lgpio.gpio_read(h, ln) == 1:
            if time.monotonic() - t0 > 0.0002:
                return {"error": "no DHT11 response (never went low)"}

        # === Measure response LOW duration ===
        t0 = time.monotonic()
        while lgpio.gpio_read(h, ln) == 0:
            if time.monotonic() - t0 > 0.001:  # 1ms
                low_dur = (time.monotonic() - t0) * 1_000_000
                return {"error": f"DHT11 response low {low_dur:.0f}µs (expected ~80µs). "
                         "Possible missing pull-up resistor on DATA line."}

        # === Wait for response HIGH ~80µs ===
        t0 = time.monotonic()
        while lgpio.gpio_read(h, ln) == 1:
            if time.monotonic() - t0 > 0.001:
                return {"error": "DHT11 response high too long"}

        # === Read 40 data bits ===
        bits = []
        for i in range(40):
            # Wait for LOW period to end (rising edge)
            t0 = time.monotonic()
            while lgpio.gpio_read(h, ln) == 0:
                if time.monotonic() - t0 > 0.0001:
                    return {"error": f"timeout low bit {i}"}

            # Measure HIGH duration
            t1 = time.monotonic()
            while lgpio.gpio_read(h, ln) == 1:
                if time.monotonic() - t1 > 0.0001:
                    return {"error": f"timeout high bit {i}"}

            high_us = (time.monotonic() - t1) * 1_000_000
            bits.append(1 if high_us > 40 else 0)

        # === Pack into 5 bytes ===
        data = [0] * 5
        for i in range(40):
            data[i // 8] = (data[i // 8] << 1) | bits[i]

        # === Checksum ===
        check = (data[0] + data[1] + data[2] + data[3]) & 0xFF
        if check != data[4]:
            return {
                "error": f"checksum mismatch: got {data[4]}, expected {check}",
                "raw": [hex(b) for b in data],
            }

        return {
            "humidity_pct": data[0],
            "temperature_c": data[2],
            "checksum_ok": True,
        }

    except Exception as e:
        return {"error": str(e)}
    finally:
        try:
            lgpio.gpiochip_close(h)
        except Exception:
            pass


def main():
    if len(sys.argv) < 2:
        print("Usage: sudo python3 dht11_read.py <gpio> [count] [--diag]", file=sys.stderr)
        sys.exit(1)

    gpio = int(sys.argv[1])
    count = 1
    diag = False
    for arg in sys.argv[2:]:
        if arg == "--diag":
            diag = True
        else:
            try:
                count = int(arg)
            except ValueError:
                pass
    count = min(count, 100)

    for i in range(count):
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        result = read_dht11(gpio, diag=diag)
        result["ts"] = ts
        result["gpio"] = gpio
        print(json.dumps(result), flush=True)

        if i < count - 1:
            time.sleep(3)


if __name__ == "__main__":
    main()
