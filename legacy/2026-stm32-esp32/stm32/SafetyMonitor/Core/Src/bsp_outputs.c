#include "bsp_outputs.h"
#include "main.h"

void BSP_Outputs_Init(void)
{
    BSP_Output_AllOff();
}

void BSP_Output_AllOff(void)
{
    BSP_Output_BuzzerOff();
    BSP_Output_RGB(0U, 0U, 0U);
    BSP_Output_RelaySet(0U);
    BSP_Output_FanSet(0U);
    BSP_Output_PumpSet(0U);
}

void BSP_Output_RGB(uint8_t r, uint8_t g, uint8_t b)
{
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_Output_BuzzerOn(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, BUZZER_ON_LEVEL);
}

void BSP_Output_BuzzerOff(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, BUZZER_OFF_LEVEL);
}

void BSP_Output_RelaySet(uint8_t on)
{
#ifdef RELAY1_CTRL_Pin
    HAL_GPIO_WritePin(RELAY1_CTRL_GPIO_Port,
                      RELAY1_CTRL_Pin,
                      on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
#else
    (void)on;
#endif
}

void BSP_Output_FanSet(uint8_t on)
{
#ifdef FAN_CTRL_Pin
    HAL_GPIO_WritePin(FAN_CTRL_GPIO_Port,
                      FAN_CTRL_Pin,
                      on ? FAN_ON_LEVEL : FAN_OFF_LEVEL);
#else
    (void)on;
#endif
}

void BSP_Output_PumpSet(uint8_t on)
{
#ifdef PUMP_CTRL_Pin
    HAL_GPIO_WritePin(PUMP_CTRL_GPIO_Port,
                      PUMP_CTRL_Pin,
                      on ? PUMP_ON_LEVEL : PUMP_OFF_LEVEL);
#else
    (void)on;
#endif
}
