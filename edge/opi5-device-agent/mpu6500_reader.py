"""MPU-6500 I2C reader for OPi5 (real hardware)."""

import os
import math

MPU6500_ADDR = 0x68
MPU6500_REG_WHO_AM_I = 0x75
MPU6500_REG_ACCEL_XOUT_H = 0x3B
MPU6500_REG_GYRO_XOUT_H = 0x43
MPU6500_REG_TEMP_H = 0x41
MPU6500_REG_PWR_MGMT_1 = 0x6B
MPU6500_REG_CONFIG = 0x1A
MPU6500_REG_ACCEL_CONFIG = 0x1C
MPU6500_REG_GYRO_CONFIG = 0x1B

MPU6500_WHO_AM_I_VALUE = 0x70

I2C_BUS_DEFAULT = 1


class MPU6500Reader:
    """Read accelerometer/gyro/temperature from MPU-6500 via I2C."""

    def __init__(self, bus_number=None):
        self._bus = None
        self._bus_number = bus_number or int(os.environ.get("MPU6500_I2C_BUS", str(I2C_BUS_DEFAULT)))
        self._available = False
        self._init()

    def _init(self):
        try:
            import smbus2
            self._bus = smbus2.SMBus(self._bus_number)
            who = self._bus.read_byte_data(MPU6500_ADDR, MPU6500_REG_WHO_AM_I)
            if who != MPU6500_WHO_AM_I_VALUE:
                print(f"[mpu6500] WHO_AM_I mismatch: got 0x{who:02X}, expected 0x{MPU6500_WHO_AM_I_VALUE:02X}")
                self._bus.close()
                self._bus = None
                return
            # Wake up: clear sleep bit
            self._bus.write_byte_data(MPU6500_ADDR, MPU6500_REG_PWR_MGMT_1, 0x01)
            # Accel ±2g, gyro ±250°s (defaults)
            self._bus.write_byte_data(MPU6500_ADDR, MPU6500_REG_ACCEL_CONFIG, 0x00)
            self._bus.write_byte_data(MPU6500_ADDR, MPU6500_REG_GYRO_CONFIG, 0x00)
            # DLPF config
            self._bus.write_byte_data(MPU6500_ADDR, MPU6500_REG_CONFIG, 0x03)
            self._available = True
            print(f"[mpu6500] initialized on /dev/i2c-{self._bus_number} addr=0x{MPU6500_ADDR:02X}")
        except ImportError:
            print("[mpu6500] smbus2 not installed, using mock data")
        except Exception as e:
            print(f"[mpu6500] init failed: {e}")
            if self._bus:
                try:
                    self._bus.close()
                except Exception:
                    pass
                self._bus = None

    @property
    def available(self):
        return self._available

    def _read_word(self, reg):
        hi = self._bus.read_byte_data(MPU6500_ADDR, reg)
        lo = self._bus.read_byte_data(MPU6500_ADDR, reg + 1)
        val = (hi << 8) | lo
        if val >= 0x8000:
            val -= 0x10000
        return val

    def read(self):
        """Return (accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z) in g and °/s."""
        if not self._available:
            return None
        try:
            raw_ax = self._read_word(MPU6500_REG_ACCEL_XOUT_H)
            raw_ay = self._read_word(MPU6500_REG_ACCEL_XOUT_H + 2)
            raw_az = self._read_word(MPU6500_REG_ACCEL_XOUT_H + 4)
            raw_gx = self._read_word(MPU6500_REG_GYRO_XOUT_H)
            raw_gy = self._read_word(MPU6500_REG_GYRO_XOUT_H + 2)
            raw_gz = self._read_word(MPU6500_REG_GYRO_XOUT_H + 4)
            # ±2g → 16384 LSB/g; ±250°/s → 131 LSB/(°/s)
            ax = round(raw_ax / 16384.0, 4)
            ay = round(raw_ay / 16384.0, 4)
            az = round(raw_az / 16384.0, 4)
            gx = round(raw_gx / 131.0, 3)
            gy = round(raw_gy / 131.0, 3)
            gz = round(raw_gz / 131.0, 3)
            return ax, ay, az, gx, gy, gz
        except Exception as e:
            print(f"[mpu6500] read error: {e}")
            return None

    def close(self):
        if self._bus:
            try:
                self._bus.close()
            except Exception:
                pass
            self._bus = None
            self._available = False
