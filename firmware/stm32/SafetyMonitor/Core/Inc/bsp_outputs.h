#ifndef BSP_OUTPUTS_H
#define BSP_OUTPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define BUZZER_ON_LEVEL    GPIO_PIN_RESET
#define BUZZER_OFF_LEVEL   GPIO_PIN_SET

/* Default active-high levels for future low-voltage drivers.
 * If the actual relay module is active-low, adjust only these relay macros later. */
#define RELAY_ON_LEVEL     GPIO_PIN_SET
#define RELAY_OFF_LEVEL    GPIO_PIN_RESET
#define FAN_ON_LEVEL       GPIO_PIN_SET
#define FAN_OFF_LEVEL      GPIO_PIN_RESET
#define PUMP_ON_LEVEL      GPIO_PIN_SET
#define PUMP_OFF_LEVEL     GPIO_PIN_RESET

void BSP_Outputs_Init(void);
void BSP_Output_AllOff(void);
void BSP_Output_RGB(uint8_t r, uint8_t g, uint8_t b);
void BSP_Output_BuzzerOn(void);
void BSP_Output_BuzzerOff(void);
void BSP_Output_RelaySet(uint8_t on);
void BSP_Output_FanSet(uint8_t on);
void BSP_Output_PumpSet(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OUTPUTS_H */
