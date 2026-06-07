#ifndef BSP_INPUTS_H
#define BSP_INPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define FLAME_ACTIVE_LEVEL  GPIO_PIN_RESET
#define PIR_ACTIVE_LEVEL    GPIO_PIN_SET
#define MQ2_ACTIVE_LEVEL    GPIO_PIN_RESET
#define DOOR_OPEN_LEVEL     GPIO_PIN_SET

void BSP_Inputs_Init(void);
uint8_t BSP_Input_FlameDetected(void);
uint8_t BSP_Input_PIRDetected(void);
uint8_t BSP_Input_MQ2Detected(void);
uint8_t BSP_Input_DoorOpen(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_INPUTS_H */
