#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Application display module for P0 debug page.
 *
 * This module only formats project runtime variables and sends them to
 * the OLED BSP driver. It does not read sensors or control actuators.
 */

void AppDisplay_Init(void);

void AppDisplay_Update(uint16_t key_adc,
                       uint8_t key_id,
                       uint8_t flame_detected,
                       uint8_t pir_detected,
                       uint8_t mq2_detected,
                       uint8_t door_open,
                       uint8_t dht11_ok,
                       uint8_t temperature,
                       uint8_t humidity);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_H */
