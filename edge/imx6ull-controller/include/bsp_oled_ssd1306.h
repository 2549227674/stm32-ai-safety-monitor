/*
 * bsp_oled_ssd1306.h — SSD1306 I2C OLED driver API.
 */

#ifndef BSP_OLED_SSD1306_H
#define BSP_OLED_SSD1306_H

#include <stddef.h>

/*
 * oled_init — Initialize SSD1306 128x64 display over I2C fd.
 * fd must already be opened and have I2C_SLAVE set to the OLED address.
 * Returns 0 on success, -1 on error.
 */
int oled_init(int fd);

/*
 * oled_clear — Fill display with zeros (black).
 * Returns 0 on success, -1 on error.
 */
int oled_clear(int fd);

/*
 * oled_draw_text — Draw ASCII text at (col, line).
 * col: pixel column (0–127); line: page/row (0–7 for 128x64).
 * Uses 5x7 font with 1-pixel gap (6px per char).
 * Returns 0 on success, -1 on error.
 */
int oled_draw_text(int fd, int col, int line, const char *text);

#endif /* BSP_OLED_SSD1306_H */
