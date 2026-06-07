/*
 * oled_test.c — Standalone SSD1306 OLED test for i.MX6ULL.
 *
 * Connects to SSD1306 via I2C, initializes, clears, and draws 4 lines
 * of test text. No dependencies on Python or other services.
 *
 * Usage:
 *   oled_test --bus /dev/i2c-0 --addr 0x3c
 *
 * Expected I2C bus content:
 *   0x3C = SSD1306 OLED
 *   0x40 = PCA9685 (unaffected by this tool)
 *   0x70 = PCA9685 all-call (reserved, unused)
 */

#include "bsp_oled_ssd1306.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--bus /dev/i2c-0] [--addr 0x3c]\n"
        "\n"
        "Test SSD1306 OLED on I2C bus. Draws 4 lines of text.\n"
        "Does not affect PCA9685 (0x40) or other I2C devices.\n",
        prog);
}

int main(int argc, char **argv) {
    const char *bus = "/dev/i2c-0";
    int addr = 0x3C;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bus") == 0 && i + 1 < argc) {
            bus = argv[++i];
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            addr = (int)strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    printf("[oled_test] bus=%s addr=0x%02X\n", bus, addr);

    int fd = open(bus, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[oled_test] open %s failed: %s\n", bus, strerror(errno));
        return 1;
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        fprintf(stderr, "[oled_test] set I2C slave 0x%02X failed: %s\n", addr, strerror(errno));
        close(fd);
        return 1;
    }

    printf("[oled_test] initializing SSD1306...\n");
    if (oled_init(fd) != 0) {
        fprintf(stderr, "[oled_test] oled_init failed\n");
        close(fd);
        return 1;
    }

    printf("[oled_test] clearing display...\n");
    if (oled_clear(fd) != 0) {
        fprintf(stderr, "[oled_test] oled_clear failed\n");
        close(fd);
        return 1;
    }

    /* Draw test pattern */
    oled_draw_text(fd, 0, 0, "Safety Monitor");
    oled_draw_text(fd, 0, 1, "Task11-A OLED");
    oled_draw_text(fd, 0, 2, "I2C OK 0x3C");
    oled_draw_text(fd, 0, 3, "P0/P1 READY");

    printf("[oled_test] display written successfully\n");
    printf("[oled_test] lines:\n");
    printf("  [0] Safety Monitor\n");
    printf("  [1] Task11-A OLED\n");
    printf("  [2] I2C OK 0x3C\n");
    printf("  [3] P0/P1 READY\n");

    close(fd);
    return 0;
}
