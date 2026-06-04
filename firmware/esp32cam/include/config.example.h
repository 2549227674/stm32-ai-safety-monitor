#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// Copy this file to include/config.h and edit local values.

// WiFi
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"

// Flask server on the same LAN.
// Do not use localhost here. Use the PC/server LAN IP, for example 192.168.1.100.
#define SERVER_HOST     "192.168.1.100"
#define SERVER_PORT     5000
#define API_ENDPOINT    "/api/events"
#define HEALTH_ENDPOINT "/health"

// Device
#define DEVICE_ID       "labbox_001"

// Timings
#define POST_INTERVAL_MS        5000UL
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define HTTP_CONNECT_TIMEOUT_MS 3000UL
#define HTTP_READ_TIMEOUT_MS    5000UL

// Stage 5 UART bridge.
// Default mode forwards STM32 UART JSON lines to Flask.
// Set ENABLE_PERIODIC_TEST_EVENT to 1 only when repeating the Stage 4 ESP32-only HTTP test.
#define ENABLE_PERIODIC_TEST_EVENT 0
#define STM32_UART_BAUD            115200
#define STM32_UART_RX_PIN          13
#define STM32_UART_TX_PIN          14
#define STM32_UART_LINE_MAX        512

// LED configuration.
// AI Thinker ESP32-CAM often has a small red status LED on GPIO33 active-low.
// GPIO4 is usually the flash LED; do not use GPIO4 as status LED unless explicitly wanted.
// Set STATUS_LED_PIN to -1 to disable LED output.
#define STATUS_LED_PIN          33
#define STATUS_LED_ACTIVE_LOW   1

#endif /* CONFIG_EXAMPLE_H */
