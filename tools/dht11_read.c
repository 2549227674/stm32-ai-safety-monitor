/*
 * dht11_read.c — Minimal DHT11 reader for OPi5/RK3588S
 * Uses lgpio for precise GPIO timing.
 * Usage: sudo ./dht11_read <gpio_number> [count]
 * Output: one JSON line per reading
 * Build: gcc -O2 -o dht11_read dht11_read.c -llgpio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <lgpio.h>

#define DHT11_START_MS   20    /* host pulls low for 20ms */
#define DHT11_TIMEOUT_US 200   /* max wait for a transition */

static int read_dht11(int gpio, int *temp, int *humid) {
    int chip, line, h;
    uint32_t bits[40];
    uint8_t data[5] = {0};
    struct timespec t0, t1;
    int64_t us;

    chip = gpio / 32;      /* gpiochip number */
    line = gpio % 32;      /* line within chip */

    h = lgpio.gpiochip_open(chip / 4);  /* map to physical chip index */
    if (h < 0) return -1;  /* can't open chip */

    /* Use the correct gpiochip for RK3588S GPIO mapping */
    /* GPIO 148 → gpiochip128 → chip index 4 → handle open(4) */
    int phys_chip = -1;
    if (gpio >= 0   && gpio < 32)   phys_chip = 0;
    if (gpio >= 32  && gpio < 64)   phys_chip = 1;
    if (gpio >= 64  && gpio < 96)   phys_chip = 2;
    if (gpio >= 96  && gpio < 128)  phys_chip = 3;
    if (gpio >= 128 && gpio < 160)  phys_chip = 4;
    if (gpio >= 160 && gpio < 512)  phys_chip = 5;
    if (phys_chip < 0) return -2;

    h = lgpio.gpiochip_open(phys_chip);
    if (h < 0) return -3;

    int ln = gpio - phys_chip * 32;

    /* Start signal: pull low for 20ms */
    lgpio.gpio_claim_output(h, ln, 0);
    lgpio.tx_pulse(h, ln, DHT11_START_MS * 1000, 0, 1);  /* low for 20ms */
    lgu_sleep(0.020);

    /* Release: set to input with pull-up */
    lgpio.gpio_free(h, ln);
    lgpio.gpio_claim_input(h, ln);

    /* Wait for DHT11 response: low for ~80µs then high for ~80µs */
    /* Skip the response signal, look for first data bit */
    lgu_sleep(0.0001);  /* 100µs settle */

    /* Read 40 bits */
    for (int i = 0; i < 40; i++) {
        /* Wait for low (start of bit) */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        while (lgpio.gpio_read(h, ln) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            us = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
            if (us > DHT11_TIMEOUT_US) { lgpio.gpiochip_close(h); return -4; }
        }
        /* Wait for high and measure duration */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        while (lgpio.gpio_read(h, ln) == 1) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            us = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
            if (us > DHT11_TIMEOUT_US) { lgpio.gpiochip_close(h); return -5; }
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        us = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
        bits[i] = (us > 40) ? 1 : 0;  /* >40µs = 1, else 0 */
    }

    lgpio.gpiochip_close(h);

    /* Pack bits into bytes */
    for (int i = 0; i < 40; i++) {
        data[i / 8] = (data[i / 8] << 1) | bits[i];
    }

    /* Checksum */
    uint8_t check = data[0] + data[1] + data[2] + data[3];
    if (check != data[4]) return -6;

    *humid = data[0];  /* integer humidity */
    *temp  = data[2];  /* integer temperature */

    return 0;  /* success */
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sudo %s <gpio_number> [count]\n", argv[0]);
        return 1;
    }

    int gpio = atoi(argv[1]);
    int count = (argc > 2) ? atoi(argv[2]) : 1;
    if (count < 1) count = 1;
    if (count > 100) count = 100;

    for (int i = 0; i < count; i++) {
        int temp = -1, humid = -1;
        int err = read_dht11(gpio, &temp, &humid);

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        char tbuf[64];
        struct tm *tm = gmtime(&ts.tv_sec);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S", tm);

        if (err == 0) {
            printf("{\"ts\":\"%sZ\",\"gpio\":%d,\"temperature_c\":%d,\"humidity_pct\":%d,\"checksum_ok\":true,\"error\":null}\n",
                   tbuf, gpio, temp, humid);
        } else {
            const char *errmsg = "unknown";
            switch (err) {
                case -1: errmsg = "can not open gpiochip"; break;
                case -2: errmsg = "invalid gpio range"; break;
                case -3: errmsg = "gpiochip open failed"; break;
                case -4: errmsg = "timeout waiting for low"; break;
                case -5: errmsg = "timeout waiting for high"; break;
                case -6: errmsg = "checksum mismatch"; break;
            }
            printf("{\"ts\":\"%sZ\",\"gpio\":%d,\"temperature_c\":null,\"humidity_pct\":null,\"checksum_ok\":false,\"error\":\"%s\"}\n",
                   tbuf, gpio, errmsg);
        }

        if (i < count - 1) lgu_sleep(3.0);
    }
    return 0;
}
