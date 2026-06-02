# STM32 固件开发说明

## 开发环境

- MCU：STM32F103C8T6
- 工具链：STM32CubeMX + Keil MDK-ARM
- 固件库：STM32 HAL 库
- 调试下载：ST-Link
- 调度方式：裸机前后台 + SysTick 定时调度

不使用 STM32F10x 标准外设库，不混用标准库和 HAL，不引入 FreeRTOS。CubeMX 生成文件中的自定义代码必须写在 `USER CODE BEGIN` / `USER CODE END` 区域。

`.c/.h` 源码注释建议使用英文，避免 Keil 中文注释显示为 `???`；README 和 docs 文档继续使用中文。

## 当前真实进度

| 编号 | 任务 | 状态 | 备注 |
|---|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED 闪烁 | 已完成 | CubeMX + Keil + HAL + ST-Link 跑通；HSE 8MHz + PLL x9，SYSCLK 72MHz；PA13/PA14 保留 SWD |
| P0-01 | USART1 串口日志输出 | 暂缓 | USB-TTL 未到货；计划使用 PA9/PA10；当前依赖 PC13 LED 和 Keil Debug Watch |
| P0-02 | AD 四位按键输入验证 | 已完成 | AD 接 PA0/ADC1_IN0；ADC1 软件触发、单通道、轮询读取；K1-K4 阈值已实测 |
| P0-03 | 蜂鸣器 / RGB LED 输出验证 | 已完成 | PB5 蜂鸣器低电平触发；PB0/PB1/PB10 分别控制 RGB R/G/B |
| P0-04 | 传感器输入验证 | 下一步 | 建议优先火焰 DO、PIR、门磁、MQ-2 AO |

当前 P0 阶段使用面包板分配 3.3V/GND。ST-Link 3.3V 只适合 STM32 和小功耗模块 bring-up；后续 ESP32-CAM、继电器、风扇、水泵必须独立供电并通过驱动模块控制。

## CubeMX 建工程步骤

1. 新建 CubeMX 工程，选择 `STM32F103C8T6`。
2. 配置系统时钟，优先使用 HSE；无外部晶振时使用 HSI。
3. `SYS` 调试接口选择 `Serial Wire`。
4. 配置 LED GPIO 为推挽输出。
5. 配置按键、门磁、PIR、火焰等 GPIO 输入，并设置 Pull-up 或 Pull-down，禁止浮空。
6. 配置 USART1 作为调试串口。
7. 配置 I2C 用于 OLED 和 MPU6050。
8. 配置 ADC 用于 MQ-2 AO。
9. 配置 TIM/PWM 用于蜂鸣器、RGB、风扇等输出。
10. Project Manager 中选择 Keil MDK-ARM 工程。
11. 生成代码后，只在 `USER CODE BEGIN` / `USER CODE END` 区域添加自定义代码。
12. 在 Keil 中编译、下载，先验证 LED 闪烁；USART1 日志待 USB-TTL 到货后补测。

引脚分配以 `docs/03_硬件系统设计与供电安全.md` 和 `firmware/stm32/docs/pinmap.md` 为准。本文件只记录阶段进度。

## 目录约定

- `Core/`：CubeMX 生成的启动、初始化和主文件。
- `BSP/`：对 HAL 外设接口做项目级封装，如 GPIO、ADC、I2C、USART、TIM/PWM。
- `Drivers/`：OLED、DHT、MQ-2、MPU6050、蜂鸣器、RGB 等设备驱动。
- `App/`：任务调度、传感器管理、风险评分、状态机、执行器联动、UI 和日志。
- `Protocol/`：STM32 与 ESP32-CAM 的 UART 帧协议。
- `Config/`：阈值、周期、引脚映射和功能开关。

## P0 最小验证顺序

1. LED：验证工程、时钟、HAL Tick 和主循环。已完成。
2. 串口：输出启动日志和周期心跳。暂缓，待 USB-TTL 到货。
3. 按键：AD 四位按键接 PA0/ADC1_IN0，验证短按和消抖。已完成输入识别。
4. 蜂鸣器/RGB：验证 PB5、PB0、PB1、PB10 输出。已完成。
5. 传感器：下一步进入 P0-04，依次接入火焰、PIR、门磁、MQ-2、DHT、MPU6050。
6. OLED：显示项目名、状态和基础传感器字段。
7. 状态机：接入风险评分、报警分级、声光和执行器联动。

## GPIO 输入要求

所有 GPIO 输入必须配置明确的 Pull-up 或 Pull-down，不能浮空。外部模块已带上拉/下拉时仍需在文档或代码注释中注明，避免误判输入电平。
