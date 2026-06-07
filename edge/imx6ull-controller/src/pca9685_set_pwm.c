/*
 * pca9685_set_pwm.c — Set a single PCA9685 channel to a specific duty cycle.
 * Used for RGB LED control on CH2/CH3/CH4 (Task10-E2).
 *
 * Usage:
 *   pca9685_set_pwm --bus /dev/i2c-0 --addr 0x40 --channel 2 --duty 4095
 *   pca9685_set_pwm --channel 3 --duty 0
 *
 * Duty range: 0 (fully off) to 4095 (fully on).
 * Wakes PCA9685 from SLEEP mode before setting duty.
 * Does NOT reinitialize frequency — preserves existing prescaler.
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

#define MODE1_SLEEP   0x10
#define MODE1_AI      0x20
#define MODE1_RESTART 0x80
#define MODE2_OUTDRV  0x04

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

static int pca9685_wake(int fd) {
    /* Set AI=1, SLEEP=0, clear RESTART. Preserve ALLCALL. */
    if (write_reg(fd, PCA9685_MODE1, MODE1_AI) != 0) return -1;
    usleep(500); /* oscillator stabilization */
    return 0;
}

static int set_channel_duty(int fd, int channel, uint16_t duty) {
    uint8_t reg = (uint8_t)(PCA9685_LED0_ON_L + 4 * channel);
    uint8_t buf[5];

    if (duty == 0) {
        /* Fully off: set bit 4 of OFF_H */
        buf[0] = reg;
        buf[1] = 0x00; /* ON_L */
        buf[2] = 0x00; /* ON_H */
        buf[3] = 0x00; /* OFF_L */
        buf[4] = 0x10; /* OFF_H: full_off bit */
    } else if (duty >= 4095) {
        /* Fully on: set bit 4 of ON_H */
        buf[0] = reg;
        buf[1] = 0x00; /* ON_L */
        buf[2] = 0x10; /* ON_H: full_on bit */
        buf[3] = 0x00; /* OFF_L */
        buf[4] = 0x00; /* OFF_H */
    } else {
        buf[0] = reg;
        buf[1] = 0x00; /* ON_L */
        buf[2] = 0x00; /* ON_H */
        buf[3] = (uint8_t)(duty & 0xff);       /* OFF_L */
        buf[4] = (uint8_t)((duty >> 8) & 0x0f); /* OFF_H */
    }

    if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "write channel %d PWM failed: %s\n", channel, strerror(errno));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *bus = "/dev/i2c-0";
    int addr = 0x40;
    int channel = -1;
    int duty = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bus") == 0 && i + 1 < argc) {
            bus = argv[++i];
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            addr = (int)strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--channel") == 0 && i + 1 < argc) {
            channel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duty") == 0 && i + 1 < argc) {
            duty = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: %s [--bus /dev/i2c-0] [--addr 0x40] --channel <0-15> --duty <0-4095>\n",
                argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return 2;
        }
    }

    if (channel < 0 || channel > 15) {
        fprintf(stderr, "ERROR: --channel must be 0-15\n");
        return 1;
    }
    if (duty < 0 || duty > 4095) {
        fprintf(stderr, "ERROR: --duty must be 0-4095\n");
        return 1;
    }

    int fd = open(bus, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", bus, strerror(errno));
        return 1;
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        fprintf(stderr, "set I2C slave 0x%02x failed: %s\n", addr, strerror(errno));
        close(fd);
        return 1;
    }

    if (pca9685_wake(fd) != 0) {
        fprintf(stderr, "WARNING: failed to wake PCA9685, attempting duty write anyway\n");
    }

    if (set_channel_duty(fd, channel, (uint16_t)duty) != 0) {
        close(fd);
        return 1;
    }

    close(fd);
    printf("pca9685: CH%d duty=%d\n", channel, duty);
    return 0;
}
