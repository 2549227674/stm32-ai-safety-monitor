#ifndef APP_COMM_H
#define APP_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void AppComm_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef AppComm_SendStm32Test(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_H */
