#include "bsp_oled.h"
#include "i2c.h"
#include <string.h>

/*
 * SSD1306 128x64 I2C OLED driver for STM32 HAL.
 *
 * Display coordinate:
 *   x:    0 ~ 127
 *   page: 0 ~ 7
 *
 * One page is 8 vertical pixels.
 * 6x8 ASCII display:
 *   each char uses 5 font columns + 1 blank column.
 */

#define OLED_CMD        0x00U
#define OLED_DATA       0x40U
#define OLED_FONT_W     5U
#define OLED_CHAR_W     6U

static uint8_t s_oled_buffer[OLED_WIDTH * OLED_PAGE_COUNT];
static uint8_t s_cursor_x = 0;
static uint8_t s_cursor_page = 0;

/* 5x7 ASCII font, characters 0x20 ~ 0x7E. */
static const uint8_t s_font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}  /* ~ */
};

static HAL_StatusTypeDef OLED_WriteCommand(uint8_t cmd);
static HAL_StatusTypeDef OLED_WriteData(const uint8_t *data, uint16_t size);
static void OLED_WriteBufferByte(uint8_t x, uint8_t page, uint8_t data);
static void OLED_WriteBufferChar(uint8_t x, uint8_t page, char ch);

static HAL_StatusTypeDef OLED_WriteCommand(uint8_t cmd)
{
    uint8_t packet[2];

    packet[0] = OLED_CMD;
    packet[1] = cmd;

    return HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, packet, 2, OLED_I2C_TIMEOUT);
}

static HAL_StatusTypeDef OLED_WriteData(const uint8_t *data, uint16_t size)
{
    uint8_t packet[17];
    uint16_t offset;
    uint16_t chunk;
    uint16_t i;

    if ((data == 0) || (size == 0))
    {
        return HAL_OK;
    }

    offset = 0;
    while (offset < size)
    {
        chunk = size - offset;
        if (chunk > 16U)
        {
            chunk = 16U;
        }

        packet[0] = OLED_DATA;
        for (i = 0; i < chunk; i++)
        {
            packet[i + 1U] = data[offset + i];
        }

        if (HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, packet, (uint16_t)(chunk + 1U), OLED_I2C_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return HAL_OK;
}

void BSP_OLED_Init(void)
{
    HAL_Delay(100);

    OLED_WriteCommand(0xAE); /* Display off */
    OLED_WriteCommand(0x20); /* Set memory addressing mode */
    OLED_WriteCommand(0x00); /* Horizontal addressing mode */
    OLED_WriteCommand(0xB0); /* Set page start address */
    OLED_WriteCommand(0xC8); /* COM output scan direction remapped */
    OLED_WriteCommand(0x00); /* Low column address */
    OLED_WriteCommand(0x10); /* High column address */
    OLED_WriteCommand(0x40); /* Start line address */
    OLED_WriteCommand(0x81); /* Contrast control */
    OLED_WriteCommand(0x7F);
    OLED_WriteCommand(0xA1); /* Segment remap */
    OLED_WriteCommand(0xA6); /* Normal display */
    OLED_WriteCommand(0xA8); /* Multiplex ratio */
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xA4); /* Display follows RAM */
    OLED_WriteCommand(0xD3); /* Display offset */
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0xD5); /* Display clock divide */
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xD9); /* Pre-charge period */
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDA); /* COM pins config */
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0xDB); /* VCOMH deselect level */
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0x8D); /* Charge pump */
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF); /* Display on */

    BSP_OLED_Clear();
    BSP_OLED_UpdateScreen();
}

void BSP_OLED_Clear(void)
{
    BSP_OLED_Fill(0x00);
    s_cursor_x = 0;
    s_cursor_page = 0;
}

void BSP_OLED_Fill(uint8_t data)
{
    memset(s_oled_buffer, data, sizeof(s_oled_buffer));
}

void BSP_OLED_UpdateScreen(void)
{
    uint8_t page;

    for (page = 0; page < OLED_PAGE_COUNT; page++)
    {
        OLED_WriteCommand((uint8_t)(0xB0U + page));
        OLED_WriteCommand(0x00);
        OLED_WriteCommand(0x10);
        OLED_WriteData(&s_oled_buffer[OLED_WIDTH * page], OLED_WIDTH);
    }
}

void BSP_OLED_SetCursor(uint8_t x, uint8_t page)
{
    if (x >= OLED_WIDTH)
    {
        x = 0;
    }

    if (page >= OLED_PAGE_COUNT)
    {
        page = 0;
    }

    s_cursor_x = x;
    s_cursor_page = page;
}

void BSP_OLED_ShowChar(uint8_t x, uint8_t page, char ch)
{
    OLED_WriteBufferChar(x, page, ch);
}

void BSP_OLED_ShowString(uint8_t x, uint8_t page, const char *str)
{
    uint8_t cur_x;
    uint8_t cur_page;

    if (str == 0)
    {
        return;
    }

    cur_x = x;
    cur_page = page;

    while (*str != '\0')
    {
        if (*str == '\n')
        {
            cur_x = 0;
            cur_page++;
            str++;
            continue;
        }

        if ((cur_x + OLED_CHAR_W) > OLED_WIDTH)
        {
            cur_x = 0;
            cur_page++;
        }

        if (cur_page >= OLED_PAGE_COUNT)
        {
            break;
        }

        OLED_WriteBufferChar(cur_x, cur_page, *str);
        cur_x = (uint8_t)(cur_x + OLED_CHAR_W);
        str++;
    }

    s_cursor_x = cur_x;
    s_cursor_page = cur_page;
}

void BSP_OLED_ShowUInt(uint8_t x, uint8_t page, uint32_t value, uint8_t width)
{
    char buf[11];
    uint8_t i;

    if (width > 10U)
    {
        width = 10U;
    }

    for (i = 0; i < 10U; i++)
    {
        buf[i] = '0';
    }
    buf[10] = '\0';

    i = 10U;
    do
    {
        i--;
        buf[i] = (char)('0' + (value % 10U));
        value = value / 10U;
    } while ((value > 0U) && (i > 0U));

    if (width == 0U)
    {
        BSP_OLED_ShowString(x, page, &buf[i]);
    }
    else
    {
        if ((10U - i) > width)
        {
            BSP_OLED_ShowString(x, page, &buf[i]);
        }
        else
        {
            BSP_OLED_ShowString(x, page, &buf[10U - width]);
        }
    }
}

void BSP_OLED_ShowHex16(uint8_t x, uint8_t page, uint16_t value)
{
    char buf[7];
    uint8_t i;
    uint8_t digit;

    buf[0] = '0';
    buf[1] = 'x';

    for (i = 0; i < 4U; i++)
    {
        digit = (uint8_t)((value >> (12U - (i * 4U))) & 0x0FU);
        if (digit < 10U)
        {
            buf[i + 2U] = (char)('0' + digit);
        }
        else
        {
            buf[i + 2U] = (char)('A' + digit - 10U);
        }
    }

    buf[6] = '\0';
    BSP_OLED_ShowString(x, page, buf);
}

void BSP_OLED_TestPage(void)
{
    BSP_OLED_Clear();
    BSP_OLED_ShowString(0, 0, "Safety Monitor");
    BSP_OLED_ShowString(0, 1, "P0 OLED OK");
    BSP_OLED_ShowString(0, 3, "I2C1 PB6/PB7");
    BSP_OLED_ShowString(0, 5, "ADDR:0x3C");
    BSP_OLED_UpdateScreen();
}

static void OLED_WriteBufferByte(uint8_t x, uint8_t page, uint8_t data)
{
    if ((x >= OLED_WIDTH) || (page >= OLED_PAGE_COUNT))
    {
        return;
    }

    s_oled_buffer[(uint16_t)page * OLED_WIDTH + x] = data;
}

static void OLED_WriteBufferChar(uint8_t x, uint8_t page, char ch)
{
    uint8_t index;
    uint8_t i;

    if ((x >= OLED_WIDTH) || (page >= OLED_PAGE_COUNT))
    {
        return;
    }

    if ((ch < 0x20) || (ch > 0x7E))
    {
        ch = '?';
    }

    index = (uint8_t)(ch - 0x20);

    for (i = 0; i < OLED_FONT_W; i++)
    {
        if ((x + i) < OLED_WIDTH)
        {
            OLED_WriteBufferByte((uint8_t)(x + i), page, s_font5x7[index][i]);
        }
    }

    if ((x + OLED_FONT_W) < OLED_WIDTH)
    {
        OLED_WriteBufferByte((uint8_t)(x + OLED_FONT_W), page, 0x00);
    }
}
