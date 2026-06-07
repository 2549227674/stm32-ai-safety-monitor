#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    KEY_NONE = 0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_UNKNOWN
} KeyId_t;

void BSP_Key_Init(void);
uint16_t BSP_Key_ReadAdc(void);
KeyId_t BSP_Key_Decode(uint16_t adc);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
