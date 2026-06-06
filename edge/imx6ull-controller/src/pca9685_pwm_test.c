#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEFAULT_I2C_DEV "/dev/i2c-0"
#define DEFAULT_I2C_ADDR 0x40

#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_LED0_ON_L 0x06
#define PCA9685_PRESCALE 0xfe

#define MODE1_RESTART 0x80
#define MODE1_SLEEP 0x10
#define MODE1_AI 0x20
#define MODE2_OUTDRV 0x04

#define OSC_CLOCK_HZ 25000000.0
#define PWM_FREQ_HZ 50.0
#define PWM_PERIOD_US 20000.0

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [--device /dev/i2c-0] [--addr 0x40] [--loop]\n"
            "\n"
            "This test configures only PCA9685 channel 0 for empty-load PWM.\n",
            prog);
}

static int parse_addr(const char *text, int *addr) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < 0x03 || value > 0x77) {
        return -1;
    }
    *addr = (int)value;
    return 0;
}

static int write_reg(int fd, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    ssize_t written = write(fd, buf, sizeof(buf));
    if (written != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "write reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }
    return 0;
}

static int read_reg(int fd, uint8_t reg, uint8_t *value) {
    ssize_t written = write(fd, &reg, 1);
    if (written != 1) {
        fprintf(stderr, "select reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }

    ssize_t read_len = read(fd, value, 1);
    if (read_len != 1) {
        fprintf(stderr, "read reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }
    return 0;
}

static uint8_t pca9685_prescale(double freq_hz) {
    double prescale = (OSC_CLOCK_HZ / (4096.0 * freq_hz)) - 1.0;
    return (uint8_t)(prescale + 0.5);
}

static uint16_t pulse_us_to_ticks(double pulse_us) {
    double ticks = (pulse_us * 4096.0) / PWM_PERIOD_US;
    if (ticks < 0.0) {
        ticks = 0.0;
    }
    if (ticks > 4095.0) {
        ticks = 4095.0;
    }
    return (uint16_t)(ticks + 0.5);
}

static int pca9685_init_50hz(int fd) {
    uint8_t old_mode = 0;
    if (read_reg(fd, PCA9685_MODE1, &old_mode) != 0) {
        return -1;
    }

    uint8_t sleep_mode = (uint8_t)((old_mode & (uint8_t)~MODE1_RESTART) | MODE1_SLEEP | MODE1_AI);
    if (write_reg(fd, PCA9685_MODE1, sleep_mode) != 0) {
        return -1;
    }

    uint8_t prescale = pca9685_prescale(PWM_FREQ_HZ);
    if (write_reg(fd, PCA9685_PRESCALE, prescale) != 0) {
        return -1;
    }

    if (write_reg(fd, PCA9685_MODE2, MODE2_OUTDRV) != 0) {
        return -1;
    }

    uint8_t wake_mode = (uint8_t)((old_mode & (uint8_t)~MODE1_SLEEP) | MODE1_AI);
    if (write_reg(fd, PCA9685_MODE1, wake_mode) != 0) {
        return -1;
    }

    usleep(5000);

    if (write_reg(fd, PCA9685_MODE1, (uint8_t)(wake_mode | MODE1_RESTART)) != 0) {
        return -1;
    }

    printf("Configured PCA9685 for %.1f Hz PWM, prescale=%u\n", PWM_FREQ_HZ, prescale);
    return 0;
}

static int set_channel0_pulse(int fd, double pulse_us) {
    uint16_t off_count = pulse_us_to_ticks(pulse_us);
    uint8_t reg = PCA9685_LED0_ON_L;
    uint8_t buf[5] = {
        reg,
        0x00,
        0x00,
        (uint8_t)(off_count & 0xff),
        (uint8_t)((off_count >> 8) & 0x0f),
    };

    ssize_t written = write(fd, buf, sizeof(buf));
    if (written != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "write channel 0 PWM failed: %s\n", strerror(errno));
        return -1;
    }

    printf("channel=0 pulse_us=%.0f ticks=%u\n", pulse_us, off_count);
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv) {
    const char *device = DEFAULT_I2C_DEV;
    int addr = DEFAULT_I2C_ADDR;
    bool loop = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--device requires a path\n");
                return 2;
            }
            device = argv[i];
        } else if (strcmp(argv[i], "--addr") == 0) {
            if (++i >= argc || parse_addr(argv[i], &addr) != 0) {
                fprintf(stderr, "--addr requires an I2C address such as 0x40\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--loop") == 0) {
            loop = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    printf("空载 PWM 测试：不要接 MG90，不要接 PCA9685 V+。\n");
    printf("Only PCA9685 channel 0 will output empty-load PWM.\n");
    printf("Do not connect servo, MOS, fan, or pump.\n");

    int fd = open(device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", device, strerror(errno));
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        fprintf(stderr, "set I2C slave 0x%02x on %s failed: %s\n", addr, device, strerror(errno));
        close(fd);
        return 1;
    }

    printf("Using %s addr=0x%02x\n", device, addr);

    if (pca9685_init_50hz(fd) != 0) {
        close(fd);
        return 1;
    }

    const double pulses_us[] = {1000.0, 1500.0, 2000.0};
    const int pulse_count = (int)(sizeof(pulses_us) / sizeof(pulses_us[0]));
    int cycles = loop ? -1 : 3;
    int cycle = 0;

    while (loop || cycle < cycles) {
        cycle++;
        printf("cycle=%d\n", cycle);
        for (int i = 0; i < pulse_count; i++) {
            if (set_channel0_pulse(fd, pulses_us[i]) != 0) {
                close(fd);
                return 1;
            }
            sleep(2);
        }
    }

    close(fd);
    printf("empty-load PWM test complete\n");
    return 0;
}
