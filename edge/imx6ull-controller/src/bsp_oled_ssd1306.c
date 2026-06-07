/*
 * bsp_oled_ssd1306.c — SSD1306 I2C OLED driver for 128x64 display.
 *
 * Minimal driver: init, clear, draw text at (col, line).
 * Uses 5x7 font from font5x7.h, 6-pixel char width (5 + 1 gap).
 * Fits 21 chars per line, 8 lines on 128x64.
 */

#include "bsp_oled_ssd1306.h"
#include "font5x7.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

/* SSD1306 commands */
#define SSD1306_SET_CONTRAST        0x81
#define SSD1306_DISPLAY_ON          0xAF
#define SSD1306_DISPLAY_OFF         0xAE
#define SSD1306_SET_DISPLAY_CLOCK   0xD5
#define SSD1306_SET_MULTIPLEX       0xA8
#define SSD1306_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_SET_START_LINE      0x40
#define SSD1306_CHARGE_PUMP         0x8D
#define SSD1306_MEMORY_MODE         0x20
#define SSD1306_SEG_REMAP           0xA1
#define SSD1306_COM_SCAN_DEC        0xC8
#define SSD1306_SET_COM_PINS        0xDA
#define SSD1306_SET_PRECHARGE       0xD9
#define SSD1306_SET_VCOMH           0xDB
#define SSD1306_DEACTIVATE_SCROLL   0x2E

#define SSD1306_CONTROL_BYTE_CMD    0x00
#define SSD1306_CONTROL_BYTE_DATA   0x40

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_PAGES  (OLED_HEIGHT / 8)
#define CHAR_WIDTH  (FONT_W + 1)  /* 5 pixels + 1 gap */
#define MAX_CHARS   (OLED_WIDTH / CHAR_WIDTH)  /* 21 */

static int oled_write_cmd(int fd, uint8_t cmd) {
    uint8_t buf[2] = {SSD1306_CONTROL_BYTE_CMD, cmd};
    if (write(fd, buf, 2) != 2) return -1;
    return 0;
}

static int oled_write_data(int fd, const uint8_t *data, size_t len) {
    /* Send in chunks to avoid I2C buffer limits */
    uint8_t buf[129];
    size_t pos = 0;
    while (pos < len) {
        size_t chunk = len - pos;
        if (chunk > 128) chunk = 128;
        buf[0] = SSD1306_CONTROL_BYTE_DATA;
        memcpy(buf + 1, data + pos, chunk);
        if (write(fd, buf, chunk + 1) != (ssize_t)(chunk + 1)) return -1;
        pos += chunk;
    }
    return 0;
}

int oled_init(int fd) {
    /* Init sequence for 128x64 SSD1306 */
    if (oled_write_cmd(fd, SSD1306_DISPLAY_OFF) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_DISPLAY_CLOCK) != 0) return -1;
    if (oled_write_cmd(fd, 0x80) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_MULTIPLEX) != 0) return -1;
    if (oled_write_cmd(fd, 0x3F) != 0) return -1;  /* 64-1 */
    if (oled_write_cmd(fd, SSD1306_SET_DISPLAY_OFFSET) != 0) return -1;
    if (oled_write_cmd(fd, 0x00) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_START_LINE | 0x00) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_CHARGE_PUMP) != 0) return -1;
    if (oled_write_cmd(fd, 0x14) != 0) return -1;  /* enable charge pump */
    if (oled_write_cmd(fd, SSD1306_MEMORY_MODE) != 0) return -1;
    if (oled_write_cmd(fd, 0x00) != 0) return -1;  /* horizontal addressing */
    if (oled_write_cmd(fd, SSD1306_SEG_REMAP) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_COM_SCAN_DEC) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_COM_PINS) != 0) return -1;
    if (oled_write_cmd(fd, 0x12) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_PRECHARGE) != 0) return -1;
    if (oled_write_cmd(fd, 0xF1) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_SET_VCOMH) != 0) return -1;
    if (oled_write_cmd(fd, 0x40) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_DEACTIVATE_SCROLL) != 0) return -1;
    if (oled_write_cmd(fd, SSD1306_DISPLAY_ON) != 0) return -1;
    return 0;
}

int oled_clear(int fd) {
    /* Set addressing window to full screen */
    if (oled_write_cmd(fd, 0x21) != 0) return -1; /* column address */
    if (oled_write_cmd(fd, 0) != 0) return -1;
    if (oled_write_cmd(fd, OLED_WIDTH - 1) != 0) return -1;
    if (oled_write_cmd(fd, 0x22) != 0) return -1; /* page address */
    if (oled_write_cmd(fd, 0) != 0) return -1;
    if (oled_write_cmd(fd, OLED_PAGES - 1) != 0) return -1;

    /* Write zeros */
    uint8_t zeros[128];
    memset(zeros, 0, sizeof(zeros));
    for (int page = 0; page < OLED_PAGES; page++) {
        for (int col = 0; col < OLED_WIDTH; col += 128) {
            if (oled_write_data(fd, zeros, 128) != 0) return -1;
        }
    }
    return 0;
}

int oled_draw_text(int fd, int col, int line, const char *text) {
    if (line < 0 || line >= OLED_PAGES) return -1;
    if (col < 0 || col >= OLED_WIDTH) return -1;

    /* Set addressing window for this line */
    if (oled_write_cmd(fd, 0x21) != 0) return -1; /* column address */
    if (oled_write_cmd(fd, col) != 0) return -1;
    if (oled_write_cmd(fd, OLED_WIDTH - 1) != 0) return -1;
    if (oled_write_cmd(fd, 0x22) != 0) return -1; /* page address */
    if (oled_write_cmd(fd, line) != 0) return -1;
    if (oled_write_cmd(fd, line) != 0) return -1;

    int max_chars = (OLED_WIDTH - col) / CHAR_WIDTH;
    int count = 0;
    for (const char *p = text; *p && count < max_chars; p++, count++) {
        uint8_t glyph[FONT_W];
        unsigned char ch = (unsigned char)*p;
        if (ch < FONT_FIRST_CHAR || ch > FONT_LAST_CHAR) {
            memset(glyph, 0, sizeof(glyph));
        } else {
            memcpy(glyph, font5x7[ch - FONT_FIRST_CHAR], FONT_W);
        }
        if (oled_write_data(fd, glyph, FONT_W) != 0) return -1;
        /* gap column */
        uint8_t gap = 0;
        if (oled_write_data(fd, &gap, 1) != 0) return -1;
    }
    return 0;
}
