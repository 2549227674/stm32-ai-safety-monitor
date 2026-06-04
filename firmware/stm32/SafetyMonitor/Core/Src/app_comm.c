#include "app_comm.h"

#include <stdio.h>
#include <string.h>

#define APP_COMM_DEVICE_ID          "labbox_001"
#define APP_COMM_TX_TIMEOUT_MS      100U
#define APP_COMM_TX_BUFFER_SIZE     384U

static UART_HandleTypeDef *s_app_comm_uart = NULL;
static uint32_t s_app_comm_seq = 1U;

void AppComm_Init(UART_HandleTypeDef *huart)
{
    s_app_comm_uart = huart;
    s_app_comm_seq = 1U;
}

HAL_StatusTypeDef AppComm_SendStm32Test(void)
{
    char json[APP_COMM_TX_BUFFER_SIZE];
    int written;
    HAL_StatusTypeDef status;

    if (s_app_comm_uart == NULL)
    {
        return HAL_ERROR;
    }

    written = snprintf(json,
                       sizeof(json),
                       "{\"type\":\"event\","
                       "\"device_id\":\"%s\","
                       "\"seq\":%lu,"
                       "\"state\":\"STM32_TEST\","
                       "\"risk_score\":0,"
                       "\"need_snap\":false,"
                       "\"sensors\":{\"door\":0,\"pir\":0,\"flame\":0,\"mq2\":0},"
                       "\"actuators\":{\"relay\":0,\"fan\":0,\"pump\":0,"
                       "\"buzzer\":0,\"rgb_r\":0,\"rgb_g\":0,\"rgb_b\":0}}\n",
                       APP_COMM_DEVICE_ID,
                       (unsigned long)s_app_comm_seq);

    if ((written < 0) || ((size_t)written >= sizeof(json)))
    {
        return HAL_ERROR;
    }

    status = HAL_UART_Transmit(s_app_comm_uart,
                               (uint8_t *)json,
                               (uint16_t)strlen(json),
                               APP_COMM_TX_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        ++s_app_comm_seq;
    }

    return status;
}
