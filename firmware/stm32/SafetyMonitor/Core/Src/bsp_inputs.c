#include "bsp_inputs.h"
#include "main.h"

void BSP_Inputs_Init(void)
{
    /* GPIO inputs are initialized by CubeMX. */
}

uint8_t BSP_Input_FlameDetected(void)
{
    GPIO_PinState level = HAL_GPIO_ReadPin(FLAME_DO_GPIO_Port, FLAME_DO_Pin);
    return (level == FLAME_ACTIVE_LEVEL) ? 1U : 0U;
}

uint8_t BSP_Input_PIRDetected(void)
{
    GPIO_PinState level = HAL_GPIO_ReadPin(PIR_DO_GPIO_Port, PIR_DO_Pin);
    return (level == PIR_ACTIVE_LEVEL) ? 1U : 0U;
}

uint8_t BSP_Input_MQ2Detected(void)
{
    GPIO_PinState level = HAL_GPIO_ReadPin(MQ2_DO_GPIO_Port, MQ2_DO_Pin);
    return (level == MQ2_ACTIVE_LEVEL) ? 1U : 0U;
}

uint8_t BSP_Input_DoorOpen(void)
{
    GPIO_PinState level = HAL_GPIO_ReadPin(DOOR_MAG_DO_GPIO_Port, DOOR_MAG_DO_Pin);
    return (level == DOOR_OPEN_LEVEL) ? 1U : 0U;
}
