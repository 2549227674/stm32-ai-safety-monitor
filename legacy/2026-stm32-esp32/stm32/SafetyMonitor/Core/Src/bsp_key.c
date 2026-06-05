#include "bsp_key.h"
#include "adc.h"
#include "stm32f1xx_hal.h"

void BSP_Key_Init(void)
{
    /* Reserved for future key initialization. ADC is initialized by CubeMX. */
}

uint16_t BSP_Key_ReadAdc(void)
{
    uint32_t sum = 0U;

    for (uint8_t i = 0U; i < 8U; i++)
    {
        HAL_ADC_Start(&hadc1);

        if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
        {
            sum += HAL_ADC_GetValue(&hadc1);
        }

        HAL_ADC_Stop(&hadc1);
        HAL_Delay(2U);
    }

    return (uint16_t)(sum / 8U);
}

KeyId_t BSP_Key_Decode(uint16_t adc)
{
    if (adc < 500U)
    {
        return KEY_NONE;
    }
    else if ((adc >= 1200U) && (adc <= 1900U))
    {
        return KEY_1;
    }
    else if ((adc >= 2100U) && (adc <= 2700U))
    {
        return KEY_2;
    }
    else if ((adc >= 3000U) && (adc <= 3500U))
    {
        return KEY_3;
    }
    else if (adc >= 3800U)
    {
        return KEY_4;
    }

    return KEY_UNKNOWN;
}
