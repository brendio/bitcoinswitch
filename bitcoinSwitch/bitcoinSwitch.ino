#include <ArduinoJson.h>
#include <WebSocketsClient.h>

// Include device-specific configuration
#ifdef WAVESHARE
#include "device_config.h"
#endif

// LED Status States - ALWAYS DEFINED (for all devices)
// Defined here so all files can use these constants
enum LEDStatus {
  STATUS_OFF,
  STATUS_BOOTING,
  STATUS_CONFIG_MODE,
  STATUS_ETHERNET_CONNECTING,
  STATUS_ETHERNET_CONNECTED,
  STATUS_WIFI_CONNECTING,
  STATUS_WIFI_CONNECTED,
  STATUS_NETWORK_ERROR,
  STATUS_WEBSOCKET_CONNECTED,
  STATUS_PAYMENT_RECEIVED,
  STATUS_RELAY_TRIGGERED,
  STATUS_ERROR
};

// Log levels (for logging system)
enum LogLevel {
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_CRITICAL
};

// Forward declarations for LED functions (from 340_status_led.ino)
void initStatusLED();
void setStatusLED(LEDStatus status);
void flashPaymentLED();
void updateStatusLED();
LEDStatus getCurrentLEDStatus();

// Forward declarations for NTP time sync functions (from 360_ntp_time.ino)
bool syncTimeWithNTP();
void maintainTimeSync();
String getCurrentTimeString();
time_t getCurrentTimestamp();

String config_ssid;
String config_password;
String config_device_string;
String config_threshold_inkey;
int config_threshold_amount;
int config_threshold_pin;
int config_threshold_time;

// Network configuration (Ethernet/WiFi)
String config_static_ip = "";
String config_static_gateway = "";
String config_static_subnet = "255.255.255.0";

// Telegram notification configuration
String telegram_bot_token = "";
String telegram_chat_id = "";
String device_name = "Waveshare-01";

// DI monitoring configuration
bool di_monitor_enabled = false;
int di_check_timeout_ms = 2000;
int di_relay_input_map[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // Relay 1-8 to DI pin mapping

// Logging configuration
bool logging_enabled = true;
int log_retention_hours = 168; // 7 days default
String syslog_server = "";
int syslog_port = 514;

// Network state tracking
bool ethernetConnected = false;
bool wifiConnected = false;

String apiUrl = "/api/v1/ws/";

WebSocketsClient webSocket;

void setup() {
    Serial.begin(115200);
    
    // UNCONDITIONAL DEBUG - This should ALWAYS print
    delay(1000);
    Serial.println("\n\n*** SETUP STARTED - FIRMWARE TIMESTAMP: 2026-01-18 15:00 ***");
    Serial.flush();
    
    #ifdef WAVESHARE
    // ESP32-S3 USB CDC requires waiting for serial connection
    // Wait up to 3 seconds for serial monitor to connect
    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }
    delay(500);  // Additional delay for stability
    #else
    delay(100);
    #endif
    
    #ifdef WAVESHARE
    Serial.println("\n\n=== Waveshare Bitcoin Switch ===");
    Serial.println("Device: " DEVICE_TYPE);
    Serial.println("Version: " DEVICE_VERSION);
    
    // Debug: Check what's defined
    #ifdef USE_RGB_LED
    Serial.println("DEBUG: USE_RGB_LED is defined");
    #else
    Serial.println("DEBUG: USE_RGB_LED is NOT defined");
    #endif
    
    #ifdef USE_ETHERNET
    Serial.println("DEBUG: USE_ETHERNET is defined");
    #endif
    
    Serial.flush();  // Ensure output is sent
    #endif
    
    // Initialize RGB LED status indicator (Waveshare)
    #ifdef USE_RGB_LED
    Serial.println("DEBUG: About to call initStatusLED()");
    initStatusLED();
    Serial.println("LED initialization complete");
    #else
    Serial.println("DEBUG: Skipping LED init - USE_RGB_LED not defined");
    #endif
    
    // Initialize logging system
    initLogging();
    
    // Initialize I2C relay controller (Waveshare)
    #ifdef WAVESHARE
    if (!initRelayController()) {
        Serial.println("ERROR: Failed to initialize relay controller!");
        #ifdef USE_RGB_LED
        setStatusLED(STATUS_ERROR);
        #endif
        while(1) {
            delay(1000);
        }
    }
    #endif
    
    // Initialize display (TFT devices only, not Waveshare)
    #ifdef TFT
    setupTFT();
    printHome(false, false, false);
    #endif
    
    setupConfig();
    
    // Disable WiFi by default (prevent auto-start)
    // Will be explicitly enabled only if needed for fallback
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disabled by default (Ethernet priority)");
    
    // Network setup: Ethernet first, WiFi fallback
    bool networkConnected = false;
    
    #ifdef USE_ETHERNET
    Serial.println("\n=== Network Setup: Ethernet Priority ===");
    networkConnected = setupEthernet();
    
    if (!networkConnected) {
        Serial.println("Ethernet not available, trying WiFi fallback...");
    }
    #endif
    
    // WiFi fallback: only if Ethernet failed AND WiFi credentials provided
    if (!networkConnected) {
        bool wifiEnabled = (config_ssid != "" && config_password != "");
        
        if (wifiEnabled) {
            Serial.println("Starting WiFi fallback...");
            networkConnected = setupWifi();
        } else {
            Serial.println("ℹ️  WiFi fallback disabled (no credentials configured)");
            Serial.println("   Set config_ssid and config_password to enable WiFi fallback");
        }
    }
    
    if (!networkConnected) {
        Serial.println("❌ No network connection available!");
        logCritical("NETWORK", "No network connection - Ethernet and WiFi both failed");
        #ifdef USE_RGB_LED
        setStatusLED(STATUS_NETWORK_ERROR);
        #endif
        printTFT("No network!", 21, 95);
        return;
    }
    
    // Wait for network to fully stabilize before starting WebSocket
    Serial.println("Waiting for network to stabilize...");
    delay(2000);
    
    // Initialize digital input pins AFTER network setup
    // This prevents SPI/Ethernet initialization from affecting DI pins
    #ifdef WAVESHARE
    Serial.println("Initializing digital input pins (INPUT_PULLUP for active-low)...");
    
    // Use DI_PINS array for cleaner iteration (demo pattern)
    // INPUT_PULLUP: Pins pulled high, grounding them activates (active-low)
    // Result: LED OFF when open, LED ON when grounded to DGND
    for (int i = 0; i < DI_COUNT; i++) {
        // Detach from any alternate functions first
        gpio_reset_pin((gpio_num_t)DI_PINS[i]);
        // Configure as input with pullup (active-low)
        pinMode(DI_PINS[i], INPUT_PULLUP);
    }
    
    delay(100); // Settling time after network init
    Serial.printf("Digital inputs configured: GPIO %d-%d (active-low)\n", DI_PIN_1, DI_PIN_8);
    
    // Initialize Telegram notifications (if configured)
    initTelegram();
    #endif

    pinMode(2, OUTPUT); // To blink on board LED

    if (config_device_string == "") {
        Serial.println("No device string configured!");
        printTFT("No device string!", 21, 95);
        return;
    }

    if (!config_device_string.startsWith("wss://")) {
        Serial.println("Device string does not start with wss://");
        printTFT("no wss://!", 21, 95);
        return;
    }

    String cleaned_device_string = config_device_string.substring(6); // Remove wss://
    String host = cleaned_device_string.substring(0, cleaned_device_string.indexOf('/'));
    String apiPath = cleaned_device_string.substring(cleaned_device_string.indexOf('/'));
    Serial.println("Websocket host: " + host);
    Serial.println("Websocket API Path: " + apiPath);

    if (config_threshold_amount != 0) { // Use in threshold mode
        Serial.println("Using THRESHOLD mode");
        Serial.println("Connecting to websocket: " + host + apiUrl + config_threshold_inkey);
        webSocket.beginSSL(host, 443, apiUrl + config_threshold_inkey);
    } else { // Use in normal mode
        Serial.println("Using NORMAL mode");
        Serial.println("Connecting to websocket: " + host + apiPath);
        webSocket.beginSSL(host, 443, apiPath);
    }
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);
}

void loop() {
    // Maintain network connections
    #ifdef USE_ETHERNET
    maintainEthernet();
    #endif
    
    loopWifi();
    webSocket.loop();
    
    // Maintain NTP time sync (retry if failed)
    maintainTimeSync();
    
    // Update LED animations (pulse effect for websocket connected state)
    #ifdef USE_RGB_LED
    updateStatusLED();
    #endif
}

void executePayment(uint8_t *payload) {
  printTFT("Payment received!", 21, 15);
  
  #ifdef USE_RGB_LED
  // Flash LED for payment indication
  flashPaymentLED();
  #else
  flashTFT();
  #endif

  String parts[3]; // pin, time, comment
  // format: {pin-time-comment} where comment is optional
  String payloadStr = String((char *)payload);
  int numParts = splitString(payloadStr, '-', parts, 3);

  int pin = parts[0].toInt();
  printTFT("Pin: " + String(pin), 21, 35);

  int time = parts[1].toInt();
  printTFT("Time: " + String(time), 21, 55);

  String comment = "";
  if (numParts == 3) {
      comment = parts[2];
      Serial.println("[WebSocket] received comment: " + comment);
      printTFT("Comment: " + comment, 21, 75);
  }
  Serial.println("[WebSocket] received pin: " + String(pin) + ", duration: " + String(time));

  if (config_threshold_amount != 0) {
      // If in threshold mode we check the "balance" pushed by the
      // websocket and use the pin/time preset
      // executeThreshold();
      return; // Threshold mode not implemented yet
  }

  // Trigger relay
  #ifdef WAVESHARE
  // For Waveshare: pin number is relay number (0-7 for Relay 1-8)
  // Convert to 0-based index if needed
  uint8_t relayNum = (pin >= 1 && pin <= 8) ? pin - 1 : pin;
  
  if (relayNum < RELAY_COUNT) {
      Serial.printf("Triggering Relay %d for %d ms\n", relayNum + 1, time);
      
      // Log payment event
      String logMsg = "Relay " + String(relayNum + 1) + " triggered for " + String(time) + "ms";
      if (comment != "") {
        logMsg += " (" + comment + ")";
      }
      logInfo("PAYMENT", logMsg);
      
      // Send Telegram payment notification
      sendTelegramPaymentAlert(relayNum + 1, time, comment);
      
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_RELAY_TRIGGERED);
      #endif
      
      // Trigger the relay
      triggerRelay(relayNum, time);
      
      // Monitor DI state change after relay trigger
      monitorDIAfterRelay(relayNum + 1, time);
      
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_WEBSOCKET_CONNECTED);  // Return to pulse
      #endif
  } else {
      Serial.printf("ERROR: Invalid relay number %d (valid: 1-%d)\n", pin, RELAY_COUNT);
  }
  #else
  // Original GPIO control for non-Waveshare devices
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(time);
  digitalWrite(pin, LOW);
  #endif

  printHome(true, true, false);

}

// long thresholdSum = 0;
// void executeThreshold() {
//     StaticJsonDocument<1900> doc;
//     DeserializationError error = deserializeJson(doc, payloadStr);
//     if (error) {
//         Serial.print("deserializeJson() failed: ");
//         Serial.println(error.c_str());
//         return;
//     }
//     thresholdSum = thresholdSum + doc["payment"]["amount"].toInt();
//     Serial.println("thresholdSum: " + String(thresholdSum));
//     if (thresholdSum >= (config_threshold_amount * 1000)) {
//         pinMode(config_threshold_pin, OUTPUT);
//         digitalWrite(config_threshold_pin, HIGH);
//         delay(config_threshold_time);
//         digitalWrite(config_threshold_pin, LOW);
//         thresholdSum = 0;
//     }
// }

//////////////////WEBSOCKET///////////////////
bool ping_toggle = false;
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_ERROR:
            Serial.printf("[WebSocket] Error: %s\n", payload);
            logError("WEBSOCKET", "Connection error: " + String((char*)payload));
            printHome(true, false, false);
            #ifdef USE_RGB_LED
            setStatusLED(STATUS_ERROR);
            #endif
            break;
        case WStype_DISCONNECTED:
            Serial.println("[WebSocket] Disconnected!\n");
            logWarning("WEBSOCKET", "Disconnected from server");
            printHome(true, false, false);
            #ifdef USE_RGB_LED
            // Revert to network layer status (ethernet/wifi indicator)
            // This shows the underlying network is still connected
            if (ethernetConnected) {
                setStatusLED(STATUS_ETHERNET_CONNECTED);  // Green
            } else if (wifiConnected) {
                setStatusLED(STATUS_WIFI_CONNECTED);  // Yellow
            } else {
                setStatusLED(STATUS_NETWORK_ERROR);  // Red
            }
            #endif
            break;
        case WStype_CONNECTED:
            Serial.printf("[WebSocket] Connected to url: %s\n", payload);
            logInfo("WEBSOCKET", "Connected to " + String((char*)payload));
            // send message to server when Connected
            webSocket.sendTXT("Connected");
            printHome(true, true, false);
            #ifdef USE_RGB_LED
            setStatusLED(STATUS_WEBSOCKET_CONNECTED);  // Green pulse
            #endif
            break;
        case WStype_TEXT:
            executePayment(payload);
            break;
        case WStype_BIN:
            Serial.printf("[WebSocket] Received binary data: %s\n", payload);
            break;
        case WStype_FRAGMENT_TEXT_START:
            break;
        case WStype_FRAGMENT_BIN_START:
            break;
        case WStype_FRAGMENT:
            break;
        case WStype_FRAGMENT_FIN:
            break;
        case WStype_PING:
            Serial.printf("[WebSocket] Ping!\n");
            ping_toggle = !ping_toggle;
            printHome(true, true, ping_toggle);
            // pong will be sent automatically
            break;
        case WStype_PONG:
            // is not used
            Serial.printf("[WebSocket] Pong!\n");
            printHome(true, true, true);
            break;
    }
}
