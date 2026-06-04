#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <cstring>
#include "config.h"

#ifndef ENABLE_PERIODIC_TEST_EVENT
#define ENABLE_PERIODIC_TEST_EVENT 0
#endif

#ifndef STM32_UART_BAUD
#define STM32_UART_BAUD 115200
#endif

#ifndef STM32_UART_RX_PIN
#define STM32_UART_RX_PIN 13
#endif

#ifndef STM32_UART_TX_PIN
#define STM32_UART_TX_PIN 14
#endif

#ifndef STM32_UART_LINE_MAX
#define STM32_UART_LINE_MAX 512
#endif

static uint32_t g_seq = 1;
static HardwareSerial Stm32Uart(1);


static void ledWrite(bool on)
{
#if STATUS_LED_PIN >= 0
    digitalWrite(STATUS_LED_PIN, (STATUS_LED_ACTIVE_LOW ? !on : on) ? HIGH : LOW);
#else
    (void)on;
#endif
}


static void ledBlinkOnce(uint16_t ms)
{
#if STATUS_LED_PIN >= 0
    ledWrite(false);
    delay(20);
    ledWrite(true);
    delay(ms);
    ledWrite(false);
#else
    (void)ms;
#endif
}


static bool ensureWiFiConnected()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    Serial.println();
    Serial.println("[wifi] disconnected, connecting...");
    Serial.printf("[wifi] ssid=%s\n", WIFI_SSID);

    ledWrite(false);
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS)
    {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[wifi] connected, ip=%s, rssi=%d dBm\n",
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
        ledWrite(true);
        return true;
    }

    Serial.printf("[wifi] connect timeout after %lu ms, status=%d\n",
                  (unsigned long)WIFI_CONNECT_TIMEOUT_MS,
                  WiFi.status());
    WiFi.disconnect(false);
    ledWrite(false);
    return false;
}


static bool buildTestEventJson(char *json, size_t jsonSize)
{
    const int written = snprintf(
        json,
        jsonSize,
        "{\"type\":\"event\","
        "\"device_id\":\"%s\","
        "\"seq\":%lu,"
        "\"state\":\"TEST\","
        "\"risk_score\":0,"
        "\"need_snap\":false,"
        "\"sensors\":{\"door\":0,\"pir\":0,\"flame\":0,\"mq2\":0},"
        "\"actuators\":{\"relay\":0,\"fan\":0,\"pump\":0,\"buzzer\":0,"
        "\"rgb_r\":0,\"rgb_g\":0,\"rgb_b\":0}}",
        DEVICE_ID,
        (unsigned long)g_seq);

    if (written < 0 || static_cast<size_t>(written) >= jsonSize)
    {
        Serial.printf("[json] buffer too small, written=%d, size=%u\n",
                      written,
                      static_cast<unsigned>(jsonSize));
        return false;
    }

    return true;
}


static bool postEventJson(const char *json, size_t jsonLength)
{
    if (!ensureWiFiConnected())
    {
        Serial.println("[post] skip: WiFi is not connected");
        return false;
    }

    const String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + API_ENDPOINT;
    Serial.printf("[post] url=%s\n", url.c_str());
    Serial.printf("[post] payload_len=%u\n", static_cast<unsigned>(jsonLength));

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);

    if (!http.begin(client, url))
    {
        Serial.println("[post] http.begin failed");
        http.end();
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    ledBlinkOnce(60);

    const int code = http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(json)), jsonLength);
    ledWrite(WiFi.status() == WL_CONNECTED);
    String body;
    if (code > 0)
    {
        body = http.getString();
        if (body.length() > 120)
        {
            body = body.substring(0, 120);
        }
        Serial.printf("[post] http_code=%d body=%s\n", code, body.c_str());
    }
    else
    {
        Serial.printf("[post] failed code=%d error=%s\n",
                      code,
                      http.errorToString(code).c_str());
    }

    http.end();

    return (code >= 200 && code < 300);
}


static bool postTestEvent()
{
    char json[512];
    if (!buildTestEventJson(json, sizeof(json)))
    {
        return false;
    }

    Serial.printf("[post] seq=%lu payload=%s\n", (unsigned long)g_seq, json);
    const bool ok = postEventJson(json, strlen(json));
    if (ok)
    {
        ++g_seq;
    }
    return ok;
}


static bool isValidJsonLine(const char *line, size_t length)
{
    return (length >= 2U) && (line[0] == '{') && (line[length - 1U] == '}');
}


static void handleStm32UartLine(const char *line, size_t length)
{
    if (!isValidJsonLine(line, length))
    {
        Serial.printf("[uart] drop invalid line len=%u\n", static_cast<unsigned>(length));
        return;
    }

    Serial.printf("[uart] forward line len=%u\n", static_cast<unsigned>(length));
    (void)postEventJson(line, length);
}


static void pollStm32Uart()
{
    static char line[STM32_UART_LINE_MAX];
    static size_t pos = 0U;

    while (Stm32Uart.available() > 0)
    {
        const int value = Stm32Uart.read();
        if (value < 0)
        {
            return;
        }

        const char ch = static_cast<char>(value);
        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            if (pos > 0U)
            {
                line[pos] = '\0';
                handleStm32UartLine(line, pos);
            }
            pos = 0U;
            continue;
        }

        if (pos >= (sizeof(line) - 1U))
        {
            Serial.println("[uart] line overflow, drop frame");
            pos = 0U;
            continue;
        }

        line[pos++] = ch;
    }
}


void setup()
{
    Serial.begin(115200);
    delay(800);

#if STATUS_LED_PIN >= 0
    pinMode(STATUS_LED_PIN, OUTPUT);
    ledWrite(false);
#endif

    Serial.println();
    Serial.println("=== ESP32-CAM Safety Monitor UART Bridge ===");
    Serial.println("[mode] stage 5: STM32 UART JSON -> HTTP event");
    Serial.println("[mode] no camera, no image upload, no local actuators");
    Serial.printf("[server] http://%s:%d%s\n", SERVER_HOST, SERVER_PORT, API_ENDPOINT);
    Serial.printf("[health] http://%s:%d%s\n", SERVER_HOST, SERVER_PORT, HEALTH_ENDPOINT);
    Serial.printf("[uart] rx_pin=%d tx_pin=%d baud=%lu\n",
                  STM32_UART_RX_PIN,
                  STM32_UART_TX_PIN,
                  static_cast<unsigned long>(STM32_UART_BAUD));

    Stm32Uart.begin(STM32_UART_BAUD, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);

    ensureWiFiConnected();
}


void loop()
{
    static uint32_t lastPost = 0;
    const uint32_t now = millis();

#if ENABLE_PERIODIC_TEST_EVENT
    if ((now - lastPost) >= POST_INTERVAL_MS)
    {
        lastPost = now;
        postTestEvent();
    }
#else
    (void)lastPost;
    (void)now;
#endif

    pollStm32Uart();

    delay(20);
}
