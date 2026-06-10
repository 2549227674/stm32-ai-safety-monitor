# Archive: 2026 STM32 + ESP32-CAM Stage

This directory contains legacy artifacts from the first project iteration using STM32F103C8T6 + ESP32-CAM.

## Historical Context

The first version used STM32 for local sensor control and ESP32-CAM for WiFi image upload to Flask. This architecture was archived when the project migrated to Linux-based single-board computers.

## What's Here

Original firmware code has been moved to `legacy/2026-stm32-esp32/` in the repository root.

## Key Facts

- STM32 PA9/USART1 JSON output: historically verified
- ESP32-CAM WiFi POST to Flask: historically verified
- STM32→ESP32-CAM GPIO13 bridge: never finalized
- Code in zip may not match latest session state
