#ifndef BSP_OLED_H
#define BSP_OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * SSD1306 128x64 I2C OLED BSP driver.
 *
 * Hardware default:
 *   OLED VCC -> 3.3V
 *   OLED GND -> GND
 *   OLED SCL -> PB6 / I2C1_SCL
 *   OLED SDA -> PB7 / I2C1_SDA
 *
 * CubeMX:
 *   I2C1 Standard Mode, 100 kHz, 7-bit addressing.
 *
 * Note:
 *   OLED_I2C_ADDR uses HAL 8-bit address format.
 *   If your module does not display, try changing 0x3C to 0x3D.
 */

#define OLED_WIDTH          128U
#define OLED_HEIGHT         64U
#define OLED_PAGE_COUNT     8U

#define OLED_I2C_ADDR       (0x3CU << 1)
#define OLED_I2C_TIMEOUT    100U

void BSP_OLED_Init(void);
void BSP_OLED_Clear(void);
void BSP_OLED_Fill(uint8_t data);
void BSP_OLED_UpdateScreen(void);

void BSP_OLED_SetCursor(uint8_t x, uint8_t page);
void BSP_OLED_ShowChar(uint8_t x, uint8_t page, char ch);
void BSP_OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void BSP_OLED_ShowUInt(uint8_t x, uint8_t page, uint32_t value, uint8_t width);
void BSP_OLED_ShowHex16(uint8_t x, uint8_t page, uint16_t value);

void BSP_OLED_TestPage(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_H */
