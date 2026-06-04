#include "bsp_dht11.h"
#include "main.h"
#include "tim.h"

/*
 * DHT11 single-wire timing driver.
 *
 * This file assumes:
 * - DHT11_DATA_Pin and DHT11_DATA_GPIO_Port are generated in main.h.
 * - htim2 is generated in tim.c/tim.h.
 * - TIM2 is configured as a 1 MHz counter and started before reading.
 */

#define DHT11_START_LOW_MS        18U
#define DHT11_START_HIGH_US       30U
#define DHT11_RESPONSE_TIMEOUT_US 120U
#define DHT11_BIT_TIMEOUT_US      120U
#define DHT11_BIT_ONE_THRESHOLD_US 40U

static void DHT11_SetPinOutput(void);
static void DHT11_SetPinInput(void);
static void DHT11_WritePin(GPIO_PinState state);
static GPIO_PinState DHT11_ReadPin(void);
static void DHT11_DelayUs(uint16_t us);
static BSP_DHT11_Status_t DHT11_WaitForLevel(GPIO_PinState target_level,
                                             uint16_t timeout_us);

void BSP_DHT11_Init(void)
{
    DHT11_SetPinOutput();
    DHT11_WritePin(GPIO_PIN_SET);
}

BSP_DHT11_Status_t BSP_DHT11_Read(uint8_t *temperature,
                                  uint8_t *humidity)
{
    uint8_t data[5];
    BSP_DHT11_Status_t status;

    if ((temperature == 0) || (humidity == 0))
    {
        return BSP_DHT11_ERROR;
    }

    status = BSP_DHT11_ReadRaw(data);
    if (status != BSP_DHT11_OK)
    {
        return status;
    }

    /*
     * DHT11 format:
     * data[0] = humidity integer
     * data[1] = humidity decimal, normally 0
     * data[2] = temperature integer
     * data[3] = temperature decimal, normally 0
     * data[4] = checksum
     */
    *humidity = data[0];
    *temperature = data[2];

    return BSP_DHT11_OK;
}

BSP_DHT11_Status_t BSP_DHT11_ReadRaw(uint8_t data[5])
{
    uint8_t byte_index;
    uint8_t bit_index;
    uint8_t bit_value;
    uint16_t high_time_us;
    BSP_DHT11_Status_t status;

    if (data == 0)
    {
        return BSP_DHT11_ERROR;
    }

    for (byte_index = 0U; byte_index < 5U; byte_index++)
    {
        data[byte_index] = 0U;
    }

    /*
     * MCU start signal:
     * 1. Pull DATA low for at least 18 ms.
     * 2. Release DATA high for about 20~40 us.
     * 3. Switch DATA to input and wait for DHT11 response.
     */
    DHT11_SetPinOutput();
    DHT11_WritePin(GPIO_PIN_RESET);
    HAL_Delay(DHT11_START_LOW_MS);

    DHT11_WritePin(GPIO_PIN_SET);
    DHT11_DelayUs(DHT11_START_HIGH_US);

    DHT11_SetPinInput();

    /*
     * DHT11 response:
     * - 80 us low
     * - 80 us high
     */
    status = DHT11_WaitForLevel(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US);
    if (status != BSP_DHT11_OK)
    {
        DHT11_SetPinOutput();
        DHT11_WritePin(GPIO_PIN_SET);
        return status;
    }

    status = DHT11_WaitForLevel(GPIO_PIN_SET, DHT11_RESPONSE_TIMEOUT_US);
    if (status != BSP_DHT11_OK)
    {
        DHT11_SetPinOutput();
        DHT11_WritePin(GPIO_PIN_SET);
        return status;
    }

    status = DHT11_WaitForLevel(GPIO_PIN_RESET, DHT11_RESPONSE_TIMEOUT_US);
    if (status != BSP_DHT11_OK)
    {
        DHT11_SetPinOutput();
        DHT11_WritePin(GPIO_PIN_SET);
        return status;
    }

    /*
     * Read 40 bits.
     * Each bit starts with about 50 us low.
     * Then high width decides bit value:
     * - about 26~28 us: 0
     * - about 70 us: 1
     */
    for (byte_index = 0U; byte_index < 5U; byte_index++)
    {
        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            data[byte_index] <<= 1;

            status = DHT11_WaitForLevel(GPIO_PIN_SET, DHT11_BIT_TIMEOUT_US);
            if (status != BSP_DHT11_OK)
            {
                DHT11_SetPinOutput();
                DHT11_WritePin(GPIO_PIN_SET);
                return status;
            }

            __HAL_TIM_SET_COUNTER(&htim2, 0U);

            status = DHT11_WaitForLevel(GPIO_PIN_RESET, DHT11_BIT_TIMEOUT_US);
            if (status != BSP_DHT11_OK)
            {
                DHT11_SetPinOutput();
                DHT11_WritePin(GPIO_PIN_SET);
                return status;
            }

            high_time_us = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

            if (high_time_us > DHT11_BIT_ONE_THRESHOLD_US)
            {
                bit_value = 1U;
            }
            else
            {
                bit_value = 0U;
            }

            data[byte_index] |= bit_value;
        }
    }

    DHT11_SetPinOutput();
    DHT11_WritePin(GPIO_PIN_SET);

    if (data[4] != (uint8_t)(data[0] + data[1] + data[2] + data[3]))
    {
        return BSP_DHT11_CHECKSUM_ERROR;
    }

    return BSP_DHT11_OK;
}

static void DHT11_SetPinOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

static void DHT11_SetPinInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

static void DHT11_WritePin(GPIO_PinState state)
{
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, state);
}

static GPIO_PinState DHT11_ReadPin(void)
{
    return HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin);
}

static void DHT11_DelayUs(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    while ((uint16_t)__HAL_TIM_GET_COUNTER(&htim2) < us)
    {
        /* Wait. */
    }
}

static BSP_DHT11_Status_t DHT11_WaitForLevel(GPIO_PinState target_level,
                                             uint16_t timeout_us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0U);

    while (DHT11_ReadPin() != target_level)
    {
        if ((uint16_t)__HAL_TIM_GET_COUNTER(&htim2) >= timeout_us)
        {
            return BSP_DHT11_TIMEOUT;
        }
    }

    return BSP_DHT11_OK;
}
