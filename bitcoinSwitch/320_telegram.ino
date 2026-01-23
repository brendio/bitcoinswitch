// ============================================
// Telegram Notifications for Waveshare ESP32-S3-ETH-8DI-8RO
// Sends alerts for payment events and DI monitoring failures
// ============================================

#ifdef WAVESHARE

#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Telegram API endpoint
#define TELEGRAM_API "https://api.telegram.org"

// Global state
bool telegramEnabled = false;
unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_CHECK_INTERVAL = 10000; // Check every 10 seconds

/**
 * Initialize Telegram functionality
 * Call this after config is loaded and network is connected
 */
void initTelegram() {
  if (telegram_bot_token == "" || telegram_chat_id == "") {
    Serial.println("Telegram disabled (no bot token or chat ID configured)");
    telegramEnabled = false;
    return;
  }
  
  Serial.println("Telegram notifications enabled");
  Serial.println("  Bot token: " + telegram_bot_token.substring(0, 10) + "...");
  Serial.println("  Chat ID: " + telegram_chat_id);
  Serial.println("  Device name: " + device_name);
  
  telegramEnabled = true;
  
  // Send startup message
  sendTelegramMessage("🔌 Device Online\n" + device_name + " started successfully");
}

/**
 * Send a message to Telegram
 * Non-blocking, returns immediately if send fails
 */
bool sendTelegramMessage(String message) {
  if (!telegramEnabled) {
    return false;
  }
  
  // Check if network is connected
  if (!wifiConnected && !ethernetConnected) {
    Serial.println("⚠️ Cannot send Telegram - no network connection");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation for simplicity
  
  HTTPClient http;
  String url = String(TELEGRAM_API) + "/bot" + telegram_bot_token + "/sendMessage";
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  
  // Build JSON payload
  String payload = "{\"chat_id\":\"" + telegram_chat_id + "\",\"text\":\"" + message + "\"}";
  
  Serial.println("📱 Sending Telegram message...");
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("✅ Telegram message sent");
      http.end();
      return true;
    } else {
      Serial.printf("⚠️ Telegram HTTP error: %d\n", httpCode);
      String response = http.getString();
      Serial.println("Response: " + response);
    }
  } else {
    Serial.printf("❌ Telegram connection failed: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return false;
}

/**
 * Send payment received notification
 */
void sendTelegramPaymentAlert(int relayNum, int duration_ms, String comment) {
  if (!telegramEnabled) return;
  
  String message = "💰 Payment Received\n";
  message += "Device: " + device_name + "\n";
  message += "🔌 Relay " + String(relayNum) + " triggered\n";
  message += "⏱️ Duration: " + String(duration_ms) + "ms";
  
  if (comment != "") {
    message += "\n💬 " + comment;
  }
  
  // Add timestamp
  message += "\n🕒 " + String(millis() / 1000) + "s uptime";
  
  sendTelegramMessage(message);
}

/**
 * Send DI monitoring failure alert
 */
void sendTelegramDIFailure(int relayNum, int diPin) {
  if (!telegramEnabled) return;
  
  String message = "⚠️ DI Monitoring Alert\n";
  message += "Device: " + device_name + "\n";
  message += "🔌 Relay " + String(relayNum) + " triggered\n";
  message += "❌ DI " + String(diPin) + " did not change state\n";
  message += "⚠️ Possible mechanical failure";
  
  sendTelegramMessage(message);
}

/**
 * Send DI monitoring success
 */
void sendTelegramDISuccess(int relayNum, int diPin) {
  if (!telegramEnabled) return;
  
  String message = "✅ DI Monitoring OK\n";
  message += "Device: " + device_name + "\n";
  message += "🔌 Relay " + String(relayNum) + "\n";
  message += "✅ DI " + String(diPin) + " changed correctly";
  
  sendTelegramMessage(message);
}

#else
// Stub functions when not using Waveshare

void initTelegram() {}
bool sendTelegramMessage(String message) { return false; }
void sendTelegramPaymentAlert(int relayNum, int duration_ms, String comment) {}
void sendTelegramDIFailure(int relayNum, int diPin) {}
void sendTelegramDISuccess(int relayNum, int diPin) {}

#endif
