/*
 * pca9685_set_pose.c — Set PCA9685 CH0 (pan) and CH1 (tilt) to specified
 * pulse width and hold. Used by pan_tilt_scan_once.sh for Task08-A.
 *
 * Safety: pulse_us limited to 1100–1900us. Tilt set before pan.
 * PWM remains active on exit (last pose held).
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define PCA9685_MODE1      0x00
#define PCA9685_MODE2      0x01
#define PCA9685_LED0_ON_L  0x06
#define PCA9685_PRESCALE   0xfe

#define MODE1_RESTART 0x80
#define MODE1_SLEEP   0x10
#define MODE1_AI      0x20
#define MODE2_OUTDRV  0x04

#define OSC_CLOCK_HZ   25000000.0
#define PWM_FREQ_HZ    50.0
#define PWM_PERIOD_US  20000.0

#define CH_PAN  0
#define CH_TILT 1

#define PULSE_MIN 1100.0
#define PULSE_MAX 1900.0

static int write_reg(int fd, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "write reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }
    return 0;
}

static int read_reg(int fd, uint8_t reg, uint8_t *value) {
    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, value, 1) != 1) return -1;
    return 0;
}

static uint16_t pulse_us_to_ticks(double pulse_us) {
    double ticks = (pulse_us * 4096.0) / PWM_PERIOD_US;
    if (ticks < 0.0) ticks = 0.0;
    if (ticks > 4095.0) ticks = 4095.0;
    return (uint16_t)(ticks + 0.5);
}

static int pca9685_init_50hz(int fd) {
    uint8_t old_mode = 0;
    if (read_reg(fd, PCA9685_MODE1, &old_mode) != 0) return -1;

    uint8_t sleep_mode = (uint8_t)((old_mode & ~MODE1_RESTART) | MODE1_SLEEP | MODE1_AI);
    if (write_reg(fd, PCA9685_MODE1, sleep_mode) != 0) return -1;

    uint8_t prescale = (uint8_t)(((OSC_CLOCK_HZ / (4096.0 * PWM_FREQ_HZ)) - 1.0) + 0.5);
    if (write_reg(fd, PCA9685_PRESCALE, prescale) != 0) return -1;
    if (write_reg(fd, PCA9685_MODE2, MODE2_OUTDRV) != 0) return -1;

    uint8_t wake_mode = (uint8_t)((old_mode & ~MODE1_SLEEP) | MODE1_AI);
    if (write_reg(fd, PCA9685_MODE1, wake_mode) != 0) return -1;
    usleep(5000);
    if (write_reg(fd, PCA9685_MODE1, (uint8_t)(wake_mode | MODE1_RESTART)) != 0) return -1;

    return 0;
}

static int set_channel_pulse(int fd, int channel, double pulse_us) {
    uint16_t off_count = pulse_us_to_ticks(pulse_us);
    uint8_t reg = (uint8_t)(PCA9685_LED0_ON_L + 4 * channel);
    uint8_t buf[5] = {
        reg, 0x00, 0x00,
        (uint8_t)(off_count & 0xff),
        (uint8_t)((off_count >> 8) & 0x0f),
    };
    if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "write channel %d PWM failed: %s\n", channel, strerror(errno));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *device = "/dev/i2c-0";
    int addr = 0x40;
    double pan_us = 1500.0;
    double tilt_us = 1500.0;
    int hold_ms = 800;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device = argv[++i];
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            addr = (int)strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--pan-us") == 0 && i + 1 < argc) {
            pan_us = atof(argv[++i]);
        } else if (strcmp(argv[i], "--tilt-us") == 0 && i + 1 < argc) {
            tilt_us = atof(argv[++i]);
        } else if (strcmp(argv[i], "--hold-ms") == 0 && i + 1 < argc) {
            hold_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: %s [--device /dev/i2c-0] [--addr 0x40]\n"
                "         --pan-us <1100-1900> --tilt-us <1100-1900> [--hold-ms 800]\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return 2;
        }
    }

    printf("========================================\n");
    printf("pca9685_set_pose\n");
    printf("Safety: pulse range %d–%d us\n", (int)PULSE_MIN, (int)PULSE_MAX);
    printf("If 5V LED dims, servo stalls/heats, or i.MX disconnects,\n");
    printf("immediately disconnect servo V+ power.\n");
    printf("========================================\n");

    if (pan_us < PULSE_MIN || pan_us > PULSE_MAX) {
        fprintf(stderr, "ERROR: pan_us=%.0f out of safe range [%d, %d]\n",
                pan_us, (int)PULSE_MIN, (int)PULSE_MAX);
        return 1;
    }
    if (tilt_us < PULSE_MIN || tilt_us > PULSE_MAX) {
        fprintf(stderr, "ERROR: tilt_us=%.0f out of safe range [%d, %d]\n",
                tilt_us, (int)PULSE_MIN, (int)PULSE_MAX);
        return 1;
    }

    int fd = open(device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
        return 1;
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        fprintf(stderr, "set I2C slave 0x%02x failed: %s\n", addr, strerror(errno));
        close(fd);
        return 1;
    }

    if (pca9685_init_50hz(fd) != 0) {
        fprintf(stderr, "PCA9685 init failed\n");
        close(fd);
        return 1;
    }

    /* Set tilt first, then pan (minimize simultaneous large moves) */
    printf("  tilt_us=%.0f\n", tilt_us);
    if (set_channel_pulse(fd, CH_TILT, tilt_us) != 0) { close(fd); return 1; }

    printf("  pan_us=%.0f\n", pan_us);
    if (set_channel_pulse(fd, CH_PAN, pan_us) != 0) { close(fd); return 1; }

    printf("  hold_ms=%d\n", hold_ms);
    if (hold_ms > 0) usleep((useconds_t)hold_ms * 1000);

    /* PWM stays active on exit — last pose held */
    close(fd);
    printf("pose set: pan=%.0fus tilt=%.0fus\n", pan_us, tilt_us);
    return 0;
}
