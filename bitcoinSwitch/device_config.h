#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

// Device identification
#ifdef WAVESHARE
#define DEVICE_TYPE "ESP32-S3-ETH-8DI-8RO"
#define DEVICE_VERSION "1.0.0-waveshare"

// ============================================
// WAVESHARE ESP32-S3-ETH-8DI-8RO Pin Definitions
// ============================================

// I2C Bus Configuration
#define I2C_SDA 42
#define I2C_SCL 41
#define I2C_FREQ 400000  // 400kHz

// TCA9554PWR I2C GPIO Expander (8 Relays)
#define TCA9554_ADDRESS 0x20
#define TCA9554_INPUT_REG 0x00
#define TCA9554_OUTPUT_REG 0x01
#define TCA9554_POLARITY_REG 0x02
#define TCA9554_CONFIG_REG 0x03
#define RELAY_COUNT 8
#define RELAY_ACTIVE_HIGH true

// Relay pin mapping (via I2C expander)
// Bit 0-7 on TCA9554 = Relay 1-8
#define RELAY_1 0
#define RELAY_2 1
#define RELAY_3 2
#define RELAY_4 3
#define RELAY_5 4
#define RELAY_6 5
#define RELAY_7 6
#define RELAY_8 7

// Digital Inputs (8 available)
// Pin mapping from Waveshare demo code (WS_DIN.h)
// Consecutive GPIO 4-11 for proper LED indicator operation
// Active-LOW: INPUT_PULLUP mode, grounding to DGND activates
// LED behavior: OFF when open, ON when grounded
#define DI_COUNT 8
#define DI_PIN_1 4      // Was 1 - updated to match demo
#define DI_PIN_2 5      // Was 2 - updated to match demo
#define DI_PIN_3 6      // Was 14 - updated to match demo
#define DI_PIN_4 7      // Was 21 - updated to match demo
#define DI_PIN_5 8      // Was 47 - updated to match demo
#define DI_PIN_6 9      // Was 48 - updated to match demo
#define DI_PIN_7 10     // Was 45 - updated to match demo
#define DI_PIN_8 11     // Was 0 - updated to match demo
#define DI_ACTIVE_LOW true  // Active low inputs with internal pullup

// DI pin array for easier iteration
const uint8_t DI_PINS[DI_COUNT] = {
    DI_PIN_1, DI_PIN_2, DI_PIN_3, DI_PIN_4,
    DI_PIN_5, DI_PIN_6, DI_PIN_7, DI_PIN_8
};

// RGB LED Status Indicator (WS2812)
#define RGB_LED_PIN 38
#define RGB_LED_COUNT 1
#define RGB_LED_BRIGHTNESS 50  // 0-255

// Ethernet (W5500 on SPI)
// Pin mapping from Waveshare demo code (WS_ETH.h)
#define USE_ETHERNET 1
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_SPI_HOST SPI3_HOST
#define ETH_CS_PIN 16     // Was 10 - updated to match demo
#define ETH_SCK_PIN 15    // Was 7 - updated to match demo
#define ETH_MISO_PIN 14   // Was 3 - updated to match demo
#define ETH_MOSI_PIN 13   // Was 6 - updated to match demo
#define ETH_INT_PIN 12    // Was 4 - updated to match demo
#define ETH_RST_PIN 39    // Was 5 - updated to match demo

// USB Serial (CDC on boot)
#define DEBUG_SERIAL Serial
#define DEBUG_BAUD 115200

// Display Configuration
// NOTE: This device has NO DISPLAY - headless operation only
// Use RGB LED and serial output for status feedback
#undef TFT
#undef USE_TFT_DISPLAY
#undef USE_OLED_DISPLAY
#define USE_RGB_LED 1

// Feature Flags
#define USE_I2C_RELAY 1
#define USE_DIGITAL_INPUTS 1
#define USE_WIFI_FALLBACK 1

#else
// Default ESP32 configuration (non-Waveshare)
#define DEVICE_TYPE "ESP32"
#endif

#endif // DEVICE_CONFIG_H
