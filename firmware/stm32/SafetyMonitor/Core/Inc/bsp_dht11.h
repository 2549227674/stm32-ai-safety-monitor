#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * DHT11 BSP driver for STM32F103 HAL.
 *
 * Hardware assumption:
 * - DHT11 VCC  -> 3.3V
 * - DHT11 GND  -> GND
 * - DHT11 DATA -> PA5 / DHT11_DATA
 * - PA5 is switched between output and input by this driver.
 * - DHT11 module boards usually include a pull-up. Bare sensors need
 *   an external 10k pull-up from DATA to 3.3V.
 *
 * Timer assumption:
 * - TIM2 is configured as a free-running 1 MHz counter.
 * - Recommended CubeMX settings when TIM2 clock is 72 MHz:
 *   Prescaler = 71
 *   Counter Period = 65535
 *   Clock Source = Internal Clock
 *   NVIC interrupt = Disabled
 */

typedef enum
{
    BSP_DHT11_OK = 0,
    BSP_DHT11_ERROR = 1,
    BSP_DHT11_TIMEOUT = 2,
    BSP_DHT11_CHECKSUM_ERROR = 3
} BSP_DHT11_Status_t;

void BSP_DHT11_Init(void);

BSP_DHT11_Status_t BSP_DHT11_Read(uint8_t *temperature,
                                  uint8_t *humidity);

BSP_DHT11_Status_t BSP_DHT11_ReadRaw(uint8_t data[5]);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DHT11_H */
