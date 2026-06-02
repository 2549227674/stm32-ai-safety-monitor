# STM32 固件开发说明

## 开发环境

- MCU：STM32F103C8T6
- 工具链：STM32CubeMX + Keil MDK-ARM
- 固件库：STM32 HAL 库
- 调试下载：ST-Link
- 调度方式：裸机前后台 + SysTick 定时调度

不使用 STM32F10x 标准外设库，不混用标准库和 HAL，不引入 FreeRTOS。CubeMX 生成文件中的自定义代码必须写在 `USER CODE BEGIN` / `USER CODE END` 区域。

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
11. 生成代码后，在 `Core` 保留 CubeMX 初始化代码。
12. 在 Keil 中编译、下载，先验证 LED 闪烁和串口日志。

引脚分配以 `docs/03_硬件系统设计与供电安全.md` 为准，本文件不重复维护引脚表。

## 目录约定

- `Core/`：CubeMX 生成的启动、初始化和主文件。
- `BSP/`：对 HAL 外设接口做项目级封装，如 GPIO、ADC、I2C、USART、TIM/PWM。
- `Drivers/`：OLED、DHT、MQ-2、MPU6050、蜂鸣器、RGB 等设备驱动。
- `App/`：任务调度、传感器管理、风险评分、状态机、执行器联动、UI 和日志。
- `Protocol/`：STM32 与 ESP32-CAM 的 UART 帧协议。
- `Config/`：阈值、周期、引脚映射和功能开关。

## P0 最小验证顺序

1. LED：验证工程、时钟、HAL Tick 和主循环。
2. 串口：输出启动日志和周期心跳。
3. OLED：显示项目名、状态和基础传感器字段。
4. 按键：验证短按、消抖和布防/撤防入口。
5. 传感器：依次接入 MQ-2、火焰、PIR、门磁、DHT、MPU6050。
6. 状态机：接入风险评分、报警分级、声光和执行器联动。

## GPIO 输入要求

所有 GPIO 输入必须配置明确的 Pull-up 或 Pull-down，不能浮空。外部模块已带上拉/下拉时仍需在文档或代码注释中注明，避免误判输入电平。
