// ============================================
// DISPLAY MODULE - DISABLED FOR WAVESHARE
// ============================================
// NOTE: Waveshare ESP32-S3-ETH-8DI-8RO has NO DISPLAY
// This device is headless and uses:
// - RGB LED (GPIO 38) for visual status feedback
// - Serial output for debugging
// - Telegram notifications for remote monitoring
//
// Display functions are stubbed out to maintain compatibility
// with the original bitcoinSwitch codebase.
// ============================================

#ifdef TFT
// TFT display support for devices with displays (e.g., TDISPLAY)
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);
void setupTFT() {
  tft.init();
  Serial.println("TFT: " + String(TFT_WIDTH) + "x" + String(TFT_HEIGHT));
  Serial.println("TFT pin MISO: " + String(TFT_MISO));
  Serial.println("TFT pin CS: " + String(TFT_CS));
  Serial.println("TFT pin MOSI: " + String(TFT_MOSI));
  Serial.println("TFT pin SCLK: " + String(TFT_SCLK));
  Serial.println("TFT pin DC: " + String(TFT_DC));
  Serial.println("TFT pin RST: " + String(TFT_RST));
  Serial.println("TFT pin BL: " + String(TFT_BL));
  tft.setRotation(1);
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
}
void printTFT(String message, int x, int y) {
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(x, y);
  tft.println(message);
}
void printHome(bool wifi, bool ws, bool ping) {
  if (ping) {
    tft.fillScreen(TFT_BLUE);
  } else {
    tft.fillScreen(TFT_BLACK);
  }
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(21, 21);
  tft.println("BitcoinSwitch");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  if (wifi) {
    tft.setCursor(21, 69);
    tft.println("WiFi connected!");
  } else {
    tft.setCursor(21, 69);
    tft.setTextColor(TFT_RED);
    tft.println("No WiFi!");
  }
  tft.setCursor(21, 95);
  if (ws) {
    if (ping) {
      tft.setTextColor(TFT_WHITE);
      tft.println("WS ping!");
    } else {
      tft.setTextColor(TFT_GREEN);
      tft.println("WS connected!");
    }
  }
  else {
    tft.setTextColor(TFT_RED);
    tft.println("No WS!");
  }
}
void clearTFT() {
  tft.fillScreen(TFT_BLACK);
}
void flashTFT() {
  tft.fillScreen(TFT_GREEN);
}
#else
// Stub functions for devices without displays (including Waveshare)
// Output to serial instead of display
void setupTFT() {
  #ifdef WAVESHARE
  Serial.println("Device has NO DISPLAY (headless operation)");
  Serial.println("Status feedback via RGB LED on GPIO 38");
  #endif
}
void printTFT(String message, int x, int y) {
  // Output to serial instead of display
  Serial.print("STATUS: ");
  Serial.println(message);
}
void printHome(bool wifi, bool ws, bool ping) {
  // Output status to serial
  Serial.println("=== Status Update ===");
  Serial.printf("WiFi: %s\n", wifi ? "Connected" : "Disconnected");
  Serial.printf("WebSocket: %s\n", ws ? "Connected" : "Disconnected");
  if (ping) Serial.println("Ping received");
}
void clearTFT() {
  // No-op for headless devices
}
void flashTFT() {
  // No-op for headless devices (use RGB LED instead)
  Serial.println("FLASH: Payment received!");
}
#endif
