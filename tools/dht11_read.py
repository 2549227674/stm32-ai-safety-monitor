#!/usr/bin/env python3
"""
DHT11 reader using lgpio (Python). Must run as root.
Usage: sudo python3 dht11_read.py <gpio> [count]
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


def read_dht11(gpio: int) -> dict:
    """Read DHT11 sensor. Returns dict with temp/humid or error."""
    chip_idx = gpio // 32
    line = gpio % 32

    # Map gpio number to physical gpiochip index for RK3588S
    # gpiochip0: GPIO 0-31, gpiochip32: 32-63, ..., gpiochip128: 128-159
    phys_chip_map = {0: 0, 32: 1, 64: 2, 96: 3, 128: 4, 160: 5}
    base = (gpio // 32) * 32
    phys_chip = phys_chip_map.get(base)
    if phys_chip is None:
        return {"error": f"gpio {gpio} not in known chip range"}

    ln = gpio - base

    try:
        h = lgpio.gpiochip_open(phys_chip)
    except Exception as e:
        return {"error": f"gpiochip_open({phys_chip}) failed: {e}"}

    try:
        # Start signal: output low for 20ms
        lgpio.gpio_claim_output(h, ln, 0)
        time.sleep(0.018)  # 18ms low

        # Switch to input (release line, pull-up brings it high)
        lgpio.gpio_free(h, ln)
        lgpio.gpio_claim_input(h, ln)

        # Small delay for DHT11 response
        time.sleep(0.00005)  # 50µs

        # Read 40 bits with timeout
        bits = []
        timeout = 0.0002  # 200µs per transition timeout

        for i in range(40):
            # Wait for rising edge (end of low period)
            t0 = time.monotonic()
            while lgpio.gpio_read(h, ln) == 0:
                if time.monotonic() - t0 > timeout:
                    return {"error": f"timeout low bit {i}"}

            # Measure high duration
            t1 = time.monotonic()
            while lgpio.gpio_read(h, ln) == 1:
                if time.monotonic() - t1 > timeout:
                    return {"error": f"timeout high bit {i}"}

            duration = time.monotonic() - t1
            bits.append(1 if duration > 0.000040 else 0)  # >40µs = 1

        # Pack into bytes
        data = [0] * 5
        for i in range(40):
            data[i // 8] = (data[i // 8] << 1) | bits[i]

        # Checksum
        check = (data[0] + data[1] + data[2] + data[3]) & 0xFF
        if check != data[4]:
            return {
                "error": f"checksum mismatch: got {data[4]}, expected {check}",
                "raw": data,
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
        print("Usage: sudo python3 dht11_read.py <gpio> [count]", file=sys.stderr)
        sys.exit(1)

    gpio = int(sys.argv[1])
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    count = min(count, 100)

    for i in range(count):
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        result = read_dht11(gpio)
        result["ts"] = ts
        result["gpio"] = gpio
        print(json.dumps(result), flush=True)

        if i < count - 1:
            time.sleep(3)


if __name__ == "__main__":
    main()
