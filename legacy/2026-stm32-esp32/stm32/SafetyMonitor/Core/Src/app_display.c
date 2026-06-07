#include "app_display.h"
#include "bsp_oled.h"

#define APP_DISPLAY_VALUE_X    54U

static const char *AppDisplay_KeyName(uint8_t key_id);
static const char *AppDisplay_StateName(uint8_t flame_detected,
                                        uint8_t pir_detected,
                                        uint8_t mq2_detected,
                                        uint8_t door_open);

void AppDisplay_Init(void)
{
    BSP_OLED_Clear();

    BSP_OLED_ShowString(0, 0, "Safety Monitor");
    BSP_OLED_ShowString(0, 1, "P0 Debug Page");

    BSP_OLED_ShowString(0, 3, "KEY ADC:");
    BSP_OLED_ShowString(0, 4, "KEY ID :");
    BSP_OLED_ShowString(0, 5, "F:0 P:0 M:0 D:0");
    BSP_OLED_ShowString(0, 6, "T:--C H:--%");
    BSP_OLED_ShowString(0, 7, "STATE  :");

    BSP_OLED_UpdateScreen();
}

void AppDisplay_Update(uint16_t key_adc,
                       uint8_t key_id,
                       uint8_t flame_detected,
                       uint8_t pir_detected,
                       uint8_t mq2_detected,
                       uint8_t door_open,
                       uint8_t dht11_ok,
                       uint8_t temperature,
                       uint8_t humidity)
{
    BSP_OLED_Clear();

    BSP_OLED_ShowString(0, 0, "Safety Monitor");
    BSP_OLED_ShowString(0, 1, "P0 Debug Page");

    BSP_OLED_ShowString(0, 3, "KEY ADC:");
    BSP_OLED_ShowUInt(APP_DISPLAY_VALUE_X, 3, key_adc, 4);

    BSP_OLED_ShowString(0, 4, "KEY ID :");
    BSP_OLED_ShowString(APP_DISPLAY_VALUE_X, 4, AppDisplay_KeyName(key_id));

    BSP_OLED_ShowString(0, 5, "F:");
    BSP_OLED_ShowUInt(12, 5, flame_detected ? 1U : 0U, 1);

    BSP_OLED_ShowString(24, 5, "P:");
    BSP_OLED_ShowUInt(36, 5, pir_detected ? 1U : 0U, 1);

    BSP_OLED_ShowString(48, 5, "M:");
    BSP_OLED_ShowUInt(60, 5, mq2_detected ? 1U : 0U, 1);

    BSP_OLED_ShowString(72, 5, "D:");
    BSP_OLED_ShowUInt(84, 5, door_open ? 1U : 0U, 1);

    if (dht11_ok)
    {
        BSP_OLED_ShowString(0, 6, "T:");
        BSP_OLED_ShowUInt(12, 6, temperature, 2);
        BSP_OLED_ShowString(24, 6, "C");

        BSP_OLED_ShowString(42, 6, "H:");
        BSP_OLED_ShowUInt(54, 6, humidity, 2);
        BSP_OLED_ShowString(66, 6, "%");
    }
    else
    {
        BSP_OLED_ShowString(0, 6, "T:--C H:--%");
    }

    BSP_OLED_ShowString(0, 7, "STATE  :");
    BSP_OLED_ShowString(APP_DISPLAY_VALUE_X,
                        7,
                        AppDisplay_StateName(flame_detected,
                                             pir_detected,
                                             mq2_detected,
                                             door_open));

    BSP_OLED_UpdateScreen();
}

static const char *AppDisplay_KeyName(uint8_t key_id)
{
    switch (key_id)
    {
        case 0U:
            return "NONE";

        case 1U:
            return "K1";

        case 2U:
            return "K2";

        case 3U:
            return "K3";

        case 4U:
            return "K4";

        default:
            return "UNKN";
    }
}

static const char *AppDisplay_StateName(uint8_t flame_detected,
                                        uint8_t pir_detected,
                                        uint8_t mq2_detected,
                                        uint8_t door_open)
{
    if (flame_detected)
    {
        return "ALARM";
    }

    if (mq2_detected)
    {
        return "SMOKE";
    }

    if (door_open)
    {
        return "DOOR";
    }

    if (pir_detected)
    {
        return "PIR";
    }

    return "IDLE";
}
