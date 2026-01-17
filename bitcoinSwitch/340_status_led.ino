// ============================================
// RGB LED Status Indicator for Waveshare ESP32-S3-ETH-8DI-8RO
// Controls WS2812 addressable RGB LED on GPIO 38
// ============================================

// Note: LEDStatus enum and function declarations are in bitcoinSwitch.ino
// This ensures they're available to all files regardless of compilation order

#ifdef USE_RGB_LED
// ============================================
// RGB LED Implementation (Waveshare only)
// ============================================

#include <FastLED.h>
#include "device_config.h"

// LED array
CRGB leds[RGB_LED_COUNT];

// LED Color Macros (use RGB components to avoid CRGB type visibility issues)
#define LED_SET_OFF()      setLEDColorRGB(0, 0, 0)
#define LED_SET_BLUE()     setLEDColorRGB(0, 0, 255)
#define LED_SET_GREEN()    setLEDColorRGB(0, 255, 0)
#define LED_SET_YELLOW()   setLEDColorRGB(255, 255, 0)
#define LED_SET_RED()      setLEDColorRGB(255, 0, 0)
#define LED_SET_CYAN()     setLEDColorRGB(0, 255, 255)
#define LED_SET_WHITE()    setLEDColorRGB(255, 255, 255)
#define LED_SET_ORANGE()   setLEDColorRGB(255, 50, 0)
#define LED_SET_PURPLE()   setLEDColorRGB(128, 0, 128)

// Global state
LEDStatus currentLEDStatus = STATUS_OFF;
bool ledPulseEnabled = false;
unsigned long lastPulseUpdate = 0;
uint8_t pulsePhase = 0;

// Animation state for pulse effect
const uint16_t PULSE_SPEED = 30;  // Milliseconds per step
const uint8_t PULSE_MIN = 10;     // Minimum brightness
const uint8_t PULSE_MAX = 255;    // Maximum brightness

// Helper function to set LED color (internal use only)
// Changed to use RGB components to avoid CRGB type visibility issues
void setLEDColorRGB(uint8_t r, uint8_t g, uint8_t b) {
  leds[0] = CRGB(r, g, b);
  FastLED.show();
}

// Initialize RGB LED
void initStatusLED() {
  Serial.println("Initializing RGB LED status indicator...");
  Serial.printf("LED Pin: GPIO %d\n", RGB_LED_PIN);
  Serial.printf("LED Count: %d\n", RGB_LED_COUNT);
  Serial.printf("Brightness: %d\n", RGB_LED_BRIGHTNESS);
  
  // Initialize FastLED
  FastLED.addLeds<WS2812, RGB_LED_PIN, RGB>(leds, RGB_LED_COUNT);
  FastLED.setBrightness(RGB_LED_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  Serial.printf("RGB LED initialized on GPIO %d\n", RGB_LED_PIN);
  
  // Test the LED by flashing it briefly
  Serial.println("Testing LED with red flash...");
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(200);
  
  Serial.println("LED test complete");
  
  // Show boot animation
  setStatusLED(STATUS_BOOTING);
}

// Set LED status (solid color, no pulse)
void setStatusLED(LEDStatus status) {
  currentLEDStatus = status;
  ledPulseEnabled = false;  // Disable pulse for solid colors
  
  switch(status) {
    case STATUS_OFF:
      LED_SET_OFF();
      Serial.println("LED: OFF");
      break;
      
    case STATUS_BOOTING:
      LED_SET_BLUE();
      Serial.println("LED: BOOTING (Blue)");
      break;
      
    case STATUS_CONFIG_MODE:
      // Fast blink handled by animation in loop
      Serial.println("LED: CONFIG MODE (Blue fast blink)");
      break;
      
    case STATUS_ETHERNET_CONNECTING:
      LED_SET_GREEN();
      Serial.println("LED: ETHERNET CONNECTING (Green)");
      break;
      
    case STATUS_ETHERNET_CONNECTED:
      LED_SET_GREEN();
      Serial.println("LED: ETHERNET CONNECTED (Solid Green)");
      break;
      
    case STATUS_WIFI_CONNECTING:
      LED_SET_ORANGE();
      Serial.println("LED: WIFI CONNECTING (Orange)");
      break;
      
    case STATUS_WIFI_CONNECTED:
      LED_SET_YELLOW();
      Serial.println("LED: WIFI CONNECTED (Solid Yellow)");
      break;
      
    case STATUS_NETWORK_ERROR:
      LED_SET_RED();
      Serial.println("LED: NETWORK ERROR (Solid Red)");
      break;
      
    case STATUS_WEBSOCKET_CONNECTED:
      // Enable pulse animation
      ledPulseEnabled = true;
      pulsePhase = 0;
      Serial.println("LED: WEBSOCKET CONNECTED (Green pulse)");
      break;
      
    case STATUS_PAYMENT_RECEIVED:
      LED_SET_PURPLE();
      Serial.println("LED: PAYMENT RECEIVED (Purple)");
      break;
      
    case STATUS_RELAY_TRIGGERED:
      LED_SET_WHITE();
      Serial.println("LED: RELAY TRIGGERED (White)");
      break;
      
    case STATUS_ERROR:
      LED_SET_RED();
      Serial.println("LED: ERROR (Red)");
      break;
      
    default:
      LED_SET_OFF();
      break;
  }
}

// Flash white for payment trigger (2x flash)
void flashPaymentLED() {
  LEDStatus savedStatus = currentLEDStatus;
  
  // White flash 2x
  for(int i = 0; i < 2; i++) {
    LED_SET_WHITE();
    delay(150);
    LED_SET_OFF();
    delay(150);
  }
  
  // Return to previous status
  setStatusLED(savedStatus);
}

// Update LED animations (call this in loop())
void updateStatusLED() {
  // Handle pulse animation for websocket connected state
  if (ledPulseEnabled && currentLEDStatus == STATUS_WEBSOCKET_CONNECTED) {
    unsigned long now = millis();
    if (now - lastPulseUpdate >= PULSE_SPEED) {
      lastPulseUpdate = now;
      
      // Calculate pulse brightness using sine wave
      float phase = (pulsePhase / 255.0) * 2.0 * PI;
      uint8_t brightness = PULSE_MIN + (uint8_t)((PULSE_MAX - PULSE_MIN) * (sin(phase) + 1.0) / 2.0);
      
      // Apply green color with pulsing brightness
      leds[0] = CRGB(0, brightness, 0);  // Green pulse
      FastLED.show();
      
      pulsePhase++;
      if (pulsePhase >= 255) pulsePhase = 0;
    }
  }
  
  // Handle config mode fast blink
  if (currentLEDStatus == STATUS_CONFIG_MODE) {
    unsigned long now = millis();
    if (now - lastPulseUpdate >= 250) {  // Fast blink every 250ms
      lastPulseUpdate = now;
      static bool blinkState = false;
      blinkState = !blinkState;
      if (blinkState) {
        LED_SET_BLUE();
      } else {
        LED_SET_OFF();
      }
    }
  }
}

// Get current LED status (for debugging)
LEDStatus getCurrentLEDStatus() {
  return currentLEDStatus;
}

// Test sequence for all LED colors
void testLEDSequence() {
  Serial.println("Testing LED colors...");
  
  struct TestColor {
    const char* name;
    void (*setFunc)();
  };
  
  TestColor colors[] = {
    {"Red", []() { LED_SET_RED(); }},
    {"Green", []() { LED_SET_GREEN(); }},
    {"Blue", []() { LED_SET_BLUE(); }},
    {"Yellow", []() { LED_SET_YELLOW(); }},
    {"Cyan", []() { LED_SET_CYAN(); }},
    {"Purple", []() { LED_SET_PURPLE(); }},
    {"Orange", []() { LED_SET_ORANGE(); }},
    {"White", []() { LED_SET_WHITE(); }}
  };
  
  for (int i = 0; i < 8; i++) {
    Serial.printf("Testing: %s\n", colors[i].name);
    colors[i].setFunc();
    delay(500);
  }
  
  LED_SET_OFF();
  Serial.println("LED test complete");
}

#else
// ============================================
// Stub implementations for devices without RGB LED
// ============================================

void initStatusLED() {
  Serial.println("RGB LED not available on this device");
}

void setStatusLED(LEDStatus status) {
  // No-op
}

void flashPaymentLED() {
  // No-op
}

void updateStatusLED() {
  // No-op
}

void testLEDSequence() {
  // No-op
}

LEDStatus getCurrentLEDStatus() {
  return STATUS_OFF;
}

#endif // USE_RGB_LED
