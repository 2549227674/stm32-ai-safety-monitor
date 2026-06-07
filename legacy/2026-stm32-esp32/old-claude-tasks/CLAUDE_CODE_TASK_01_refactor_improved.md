# Claude Code 任务 01：STM32 代码模块拆分（改进版）

> 用法：复制本文件正文给本地 Claude Code / Codex 执行  
> 任务类型：代码重构，不新增业务功能  
> 位置：`firmware/stm32/SafetyMonitor/`  
> 前提：本地 Keil 工程目录已经同步到 Git 仓库，并能打开 `MDK-ARM/SafetyMonitor.uvprojx`

---

## 0. 审核改进摘要

### 0.1 原任务主要问题

1. 原任务示例调用 `AppDisplay_Update(..., s_flame, s_pir, s_mq2, s_door)` 为 6 参数，但当前 Core 里的 `app_display.h` 已经是 9 参数版本，包含 DHT11 显示参数。
2. 当前 `main.c` 已经包含 `bsp_dht11.h`、`HAL_TIM_Base_Start(&htim2)`、`BSP_DHT11_Init()` 和周期读取 DHT11 的逻辑，但项目决策是 DHT11 暂缓，任务必须明确禁用这些调用。
3. 当前 `main.h` 还没有 `RELAY1_CTRL` / `FAN_CTRL` / `PUMP_CTRL` 引脚宏。原任务要求预留输出接口，但不能直接写不存在的 GPIO 宏，否则编译失败。
4. 原任务说不修改 `app_display`，但没有说明 9 参数显示中 DHT11 失败占位如何传入。
5. 原任务没有明确 Keil 工程 `.uvprojx` 需要手动加入新增 `.c` 文件，否则 Rebuild 会链接失败。
6. 原任务没有说明 `bsp_key.c` 需要包含 `adc.h` 才能访问 `hadc1`。

### 0.2 本版补充内容

1. 明确按当前 Core 状态改造：保留 `AppDisplay_Update()` 9 参数，DHT11 暂缓时最后三个参数统一传 `0U, 0U, 0U`。
2. 明确从 `main.c` 移除 DHT11 include、初始化、读取和变量，但不删除 `bsp_dht11.h/c`。
3. `Relay/Fan/Pump` 预留接口使用 `#ifdef RELAY1_CTRL_Pin` 等条件编译，避免当前未配置 PA6/PA7/PB8 时编译失败。
4. 补充每个新模块的头文件接口、实现注意事项和测试清单。
5. 明确本任务不改 `.ioc`、不改 CubeMX 自动生成初始化代码、不新增业务功能。

---

## 1. 项目位置

```text
仓库：stm32-ai-safety-monitor
STM32 工程：firmware/stm32/SafetyMonitor/
用户代码目录：Core/Inc/ 和 Core/Src/
Keil 工程：MDK-ARM/SafetyMonitor.uvprojx
```

---

## 2. 当前已知 Core 状态

当前上传的 Core 里：

1. `main.c` 中仍有 ADKEY、FLAME、PIR、MQ2、DOOR、BUZZER、RGB 的业务函数。
2. `app_display.h` 当前接口是 9 参数：

```c
void AppDisplay_Update(uint16_t key_adc,
                       uint8_t key_id,
                       uint8_t flame_detected,
                       uint8_t pir_detected,
                       uint8_t mq2_detected,
                       uint8_t door_open,
                       uint8_t dht11_ok,
                       uint8_t temperature,
                       uint8_t humidity);
```

3. `main.c` 当前调用了 DHT11，但本项目当前决策是 DHT11 暂缓。
4. `main.h` 当前已有这些引脚宏：

```text
DOOR_MAG_DO = PA4
DHT11_DATA  = PA5
RGB_R       = PB0
RGB_G       = PB1
RGB_B       = PB10
FLAME_DO    = PB12
MQ2_DO      = PB14
PIR_DO      = PB15
BUZZER      = PB5
```

5. 当前 `main.h` 尚未包含以下 P0-05 计划引脚：

```text
RELAY1_CTRL = PA6
FAN_CTRL    = PA7
PUMP_CTRL   = PB8
```

因此本任务只能预留接口，不能假设这些宏已经存在。

---

## 3. 项目约束

1. 保留 `STM32CubeMX + Keil MDK-ARM + HAL`，不切换标准库。
2. 不修改 `.ioc` 文件。
3. 不修改 CubeMX 自动生成的初始化代码，例如 `MX_GPIO_Init()`、`MX_ADC1_Init()`、`MX_I2C1_Init()`、`MX_TIM2_Init()`。
4. 不修改已验证引脚定义。
5. 不启用 DHT11：`bsp_dht11.h/c` 可以保留，但 `main.c` 不 include、不初始化、不读取，不参与 `risk_score` 或 `safety_fsm`。
6. 不引入 FreeRTOS。
7. `P0_ENABLE_BUZZER_TEST` 保持 `0U`，蜂鸣器默认关闭。
8. 代码注释使用英文，避免 Keil 中文注释乱码。
9. 不新增业务功能；本任务只做结构化重构。

---

## 4. 任务目标

将 `main.c` 中 USER CODE 区域里的业务逻辑拆分到独立模块文件中。

拆分后：

1. `main.c` 只保留初始化、周期读取、OLED 更新和全关保护。
2. ADKEY 相关代码在 `bsp_key.h/c`。
3. 传感器数字输入读取在 `bsp_inputs.h/c`。
4. 蜂鸣器、RGB 和后续执行器输出接口在 `bsp_outputs.h/c`。
5. DHT11 相关代码不参与主循环。
6. OLED 页面继续由 `app_display.h/c` 负责。

---

## 5. 新建文件 1：`Core/Inc/bsp_key.h`

请创建：

```c
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
```

---

## 6. 新建文件 2：`Core/Src/bsp_key.c`

从 `main.c` 移动：

1. `ADKey_ReadAdc()` → `BSP_Key_ReadAdc()`
2. `ADKey_Decode()` → `BSP_Key_Decode()`

实现要求：

1. 包含 `bsp_key.h`。
2. 包含 `adc.h`，用于访问 `hadc1`。
3. 保留当前 8 次采样求平均逻辑。
4. `BSP_Key_Init()` 当前可以为空实现，保留接口。
5. 不修改 ADC 初始化代码。

建议实现结构：

```c
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
```

---

## 7. 新建文件 3：`Core/Inc/bsp_inputs.h`

请创建：

```c
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
```

---

## 8. 新建文件 4：`Core/Src/bsp_inputs.c`

从 `main.c` 移动：

1. `Flame_IsDetected()` → `BSP_Input_FlameDetected()`
2. `PIR_IsDetected()` → `BSP_Input_PIRDetected()`
3. `MQ2_IsDetected()` → `BSP_Input_MQ2Detected()`
4. `DoorMag_IsOpen()` → `BSP_Input_DoorOpen()`

实现要求：

1. 包含 `bsp_inputs.h`。
2. 包含 `main.h`，用于访问 GPIO label 宏。
3. `BSP_Inputs_Init()` 当前为空实现。
4. 不重新初始化 GPIO。
5. 不读取 DHT11。

建议实现结构：

```c
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
```

---

## 9. 新建文件 5：`Core/Inc/bsp_outputs.h`

请创建：

```c
#ifndef BSP_OUTPUTS_H
#define BSP_OUTPUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define BUZZER_ON_LEVEL    GPIO_PIN_RESET
#define BUZZER_OFF_LEVEL   GPIO_PIN_SET

/* Default active-high levels for future low-voltage drivers.
 * If the actual relay module is active-low, adjust only these relay macros later. */
#define RELAY_ON_LEVEL     GPIO_PIN_SET
#define RELAY_OFF_LEVEL    GPIO_PIN_RESET
#define FAN_ON_LEVEL       GPIO_PIN_SET
#define FAN_OFF_LEVEL      GPIO_PIN_RESET
#define PUMP_ON_LEVEL      GPIO_PIN_SET
#define PUMP_OFF_LEVEL     GPIO_PIN_RESET

void BSP_Outputs_Init(void);
void BSP_Output_AllOff(void);
void BSP_Output_RGB(uint8_t r, uint8_t g, uint8_t b);
void BSP_Output_BuzzerOn(void);
void BSP_Output_BuzzerOff(void);
void BSP_Output_RelaySet(uint8_t on);
void BSP_Output_FanSet(uint8_t on);
void BSP_Output_PumpSet(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OUTPUTS_H */
```

---

## 10. 新建文件 6：`Core/Src/bsp_outputs.c`

从 `main.c` 移动：

1. `Output_AllOff()` → `BSP_Output_AllOff()`
2. `RGB_Set()` → `BSP_Output_RGB()`

新增：

1. `BSP_Output_BuzzerOn()`
2. `BSP_Output_BuzzerOff()`
3. `BSP_Output_RelaySet()`
4. `BSP_Output_FanSet()`
5. `BSP_Output_PumpSet()`

实现要求：

1. 包含 `bsp_outputs.h`。
2. 包含 `main.h`。
3. `BSP_Output_AllOff()` 必须关闭蜂鸣器、RGB，以及未来存在的 relay/fan/pump。
4. 当前 `RELAY1_CTRL_Pin`、`FAN_CTRL_Pin`、`PUMP_CTRL_Pin` 可能还不存在，执行器接口只做预留，必须使用 `#ifdef` 条件编译；不要在未由 CubeMX 生成 PA6/PA7/PB8 宏前无条件访问这些宏。
5. 不重新初始化 GPIO。

建议实现结构：

```c
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
```

---

## 11. 保留已有模块

以下文件保留，不在本任务中重构：

```text
Core/Inc/bsp_oled.h
Core/Src/bsp_oled.c
Core/Inc/app_display.h
Core/Src/app_display.c
Core/Inc/bsp_dht11.h
Core/Src/bsp_dht11.c
```

注意：

1. `bsp_dht11.h/c` 保留但不从 `main.c` 调用。
2. `app_display` 继续显示温湿度占位：`T:--C H:--%`。
3. 后续如重新补测 DHT11，再单独开任务恢复。

---

## 12. main.c 改造要求

### 12.1 Includes

将 USER CODE Includes 改为：

```c
/* USER CODE BEGIN Includes */
#include "bsp_key.h"
#include "bsp_inputs.h"
#include "bsp_outputs.h"
#include "bsp_oled.h"
#include "app_display.h"
/* USER CODE END Includes */
```

删除或不再使用：

```c
#include "bsp_dht11.h"
```

### 12.2 删除 main.c 内部 typedef/define/prototype/function

从 `main.c` 删除或迁移：

```text
KeyId_t typedef
BUZZER_ON_LEVEL / BUZZER_OFF_LEVEL
FLAME_ACTIVE_LEVEL / PIR_ACTIVE_LEVEL / MQ2_ACTIVE_LEVEL / DOOR_OPEN_LEVEL
ADKey_ReadAdc()
ADKey_Decode()
Flame_IsDetected()
PIR_IsDetected()
MQ2_IsDetected()
DoorMag_IsOpen()
Output_AllOff()
RGB_Set()
DHT11 全部变量和读取逻辑
```

旧 `main.c` 全局变量必须删除，不要保留残余声明：

```text
g_key_adc
g_key_id
g_flame_detected
g_pir_detected
g_mq2_detected
g_door_open
g_dht11_ok
g_temperature
g_humidity
```

`P0_ENABLE_BUZZER_TEST` 可保留在 `main.c` USER CODE PD：

```c
#define P0_ENABLE_BUZZER_TEST 0U
```

但本任务建议主循环中不要恢复蜂鸣器临时测试，只持续 `BSP_Output_AllOff()`。

### 12.3 Private variables

将 USER CODE PV 改为：

```c
/* USER CODE BEGIN PV */
static uint16_t s_key_adc = 0U;
static KeyId_t s_key_id = KEY_NONE;
static uint8_t s_flame = 0U;
static uint8_t s_pir = 0U;
static uint8_t s_mq2 = 0U;
static uint8_t s_door = 0U;
/* USER CODE END PV */
```

不再保留：

```c
volatile uint16_t g_key_adc;
volatile KeyId_t g_key_id;
volatile uint8_t g_flame_detected;
volatile uint8_t g_pir_detected;
volatile uint8_t g_mq2_detected;
volatile uint8_t g_door_open;
volatile uint8_t g_dht11_ok;
volatile uint8_t g_temperature;
volatile uint8_t g_humidity;
```

说明：当前没有中断更新这些变量，新变量使用 `static`，不使用 `volatile`。

### 12.4 USER CODE BEGIN 2

改为：

```c
/* USER CODE BEGIN 2 */
BSP_Outputs_Init();

HAL_ADCEx_Calibration_Start(&hadc1);
BSP_Key_Init();
BSP_Inputs_Init();

BSP_OLED_Init();
AppDisplay_Init();
/* USER CODE END 2 */
```

不要调用：

```c
HAL_TIM_Base_Start(&htim2);
BSP_DHT11_Init();
BSP_DHT11_Read();
```

说明：`BSP_Outputs_Init()` 内部已经调用 `BSP_Output_AllOff()`，`main.c` 初始化阶段不再重复调用。`MX_TIM2_Init()` 可能仍由 CubeMX 自动生成并调用，本任务不强行删除；但不要启动 TIM2 和 DHT11 读取。

### 12.5 USER CODE BEGIN 3

改为：

```c
/* USER CODE BEGIN 3 */
s_key_adc = BSP_Key_ReadAdc();
s_key_id = BSP_Key_Decode(s_key_adc);
s_flame = BSP_Input_FlameDetected();
s_pir = BSP_Input_PIRDetected();
s_mq2 = BSP_Input_MQ2Detected();
s_door = BSP_Input_DoorOpen();

AppDisplay_Update(s_key_adc,
                  (uint8_t)s_key_id,
                  s_flame,
                  s_pir,
                  s_mq2,
                  s_door,
                  0U,
                  0U,
                  0U);

BSP_Output_AllOff();
HAL_Delay(100U);
/* USER CODE END 3 */
```

说明：

1. 第 7~9 个参数是 DHT11 占位：`dht11_ok=0`、`temperature=0`、`humidity=0`。
2. OLED 应继续显示 `T:--C H:--%`。
3. 当前重构阶段不做执行器联动，避免蜂鸣器误响。

---

## 13. Keil 工程修改要求

必须将以下 `.c` 文件加入 Keil 工程：

```text
Core/Src/bsp_key.c
Core/Src/bsp_inputs.c
Core/Src/bsp_outputs.c
```

建议放在与 `bsp_oled.c`、`app_display.c` 同一组或新建 `User/BSP` 分组。

如果不加入工程，可能出现：

```text
undefined symbol BSP_Key_ReadAdc
undefined symbol BSP_Input_DoorOpen
undefined symbol BSP_Output_AllOff
```

---

## 14. 禁止事项

1. 不要删除 `main.c` 中 CubeMX 自动生成代码。
2. 不要修改 `MX_GPIO_Init()`、`MX_ADC1_Init()`、`MX_I2C1_Init()`、`MX_TIM2_Init()`。
3. 不要修改 `.ioc` 文件。
4. 不要重新分配已验证引脚。
5. 不要调用 DHT11 相关函数。
6. 不要引入外部库。
7. 不要把 Relay/Fan/Pump 直接写成固定 GPIO，除非 `main.h` 已有对应宏。
8. 不要在 `bsp_inputs.c` / `bsp_outputs.c` 中重新初始化 GPIO。

---

## 15. 完成后请输出

Claude Code / Codex 完成后请输出：

1. 修改/新增的文件列表。
2. 每个新模块的接口说明：函数名、参数、返回值。
3. `main.c` 中 USER CODE 区域最终内容摘要。
4. Keil 工程中需要手动添加的 `.c` 文件。
5. DHT11 禁用方式说明。
6. 手工测试步骤。
7. 已知风险或未验证项。
8. 建议 commit message。

建议 commit message：

```text
refactor(stm32): split main.c into key input and output BSP modules
```

---

## 16. 验证检查清单

### 16.1 编译检查

- [ ] Keil Rebuild 0 Error。
- [ ] 无 `undefined symbol`。
- [ ] 无 `implicit declaration`。
- [ ] 无 `too few arguments to function AppDisplay_Update`。
- [ ] 新增 `.c` 文件已加入 `.uvprojx`。

### 16.2 烧录检查

- [ ] 上电蜂鸣器不误响。
- [ ] OLED 显示 `Safety Monitor / P0 Debug Page`。
- [ ] ADKEY ADC 数值变化正常。
- [ ] K1/K2/K3/K4 识别正常。
- [ ] PA4 → GND 显示 `D:0`。
- [ ] PA4 悬空或门磁远离显示 `D:1`。
- [ ] PB15/PB14/PB12 软件模拟输入仍能改变 OLED 状态。
- [ ] OLED 温湿度显示 `T:--C H:--%`，不阻塞主循环。

### 16.3 回滚原则

如果重构后无法编译：

1. 先检查新增 `.c` 是否已加入 Keil 工程。
2. 再检查 `AppDisplay_Update()` 参数是否仍为 9 个。
3. 再检查是否残留 DHT11 变量或函数调用。
4. 不要修改 CubeMX 初始化函数来“修编译”。
