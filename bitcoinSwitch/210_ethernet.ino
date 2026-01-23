// ============================================
// Ethernet Module for Waveshare ESP32-S3-ETH-8DI-8RO
// W5500 Ethernet via SPI with Event-Driven Architecture
// Based on Waveshare demo code (WS_ETH.cpp)
// ============================================

#ifdef USE_ETHERNET

#include <ETH.h>
#include <SPI.h>
#include <NetworkClientSecure.h>
#include "device_config.h"

// Global Ethernet state (ethernetConnected declared in main sketch)
bool ethernetInitialized = false;
IPAddress ETH_ip;

// FreeRTOS task handle
TaskHandle_t ethernetTaskHandle = NULL;

/**
 * Ethernet Event Handler (demo-style architecture)
 * Handles all Ethernet state changes via ESP32 event system
 */
void onEthernetEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("bitcoinswitch");  // Set device hostname
      logInfo("NETWORK", "Ethernet hardware started");
      break;
      
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected - cable detected");
      logInfo("NETWORK", "Ethernet cable connected");
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_ETHERNET_CONNECTING);
      #endif
      break;
      
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.printf("ETH Got IP from DHCP: %s\n", esp_netif_get_desc(info.got_ip.esp_netif));
      ETH_ip = ETH.localIP();
      Serial.printf("   IP Address: %d.%d.%d.%d\n", ETH_ip[0], ETH_ip[1], ETH_ip[2], ETH_ip[3]);
      Serial.printf("   Subnet:     %s\n", ETH.subnetMask().toString().c_str());
      Serial.printf("   Gateway:    %s\n", ETH.gatewayIP().toString().c_str());
      Serial.printf("   DNS:        %s\n", ETH.dnsIP().toString().c_str());
      
      logInfo("NETWORK", "Ethernet connected - IP: " + ETH_ip.toString());
      
      // If WiFi was active as fallback, Ethernet takeover
      if (wifiConnected) {
        Serial.println("Ethernet restored - taking over from WiFi fallback");
        logInfo("NETWORK", "Switching from WiFi to Ethernet (primary)");
      }
      
      ethernetConnected = true;
      wifiConnected = false;  // Clear WiFi flag - Ethernet is primary
      
      // Sync time with NTP (demo pattern)
      syncTimeWithNTP();
      
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_ETHERNET_CONNECTED);
      Serial.println("LED: Ethernet connected (green)");
      #endif
      break;
      
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP Address");
      logWarning("NETWORK", "Ethernet lost IP address");
      ethernetConnected = false;
      
      // Trigger WiFi fallback if available (after short delay)
      delay(500);  // Brief delay to allow Ethernet recovery
      if (!ethernetConnected && !wifiConnected && config_ssid != "" && config_password != "") {
        Serial.println("Ethernet IP lost, attempting WiFi fallback...");
        setupWifi();  // This will handle LED status and NTP sync
      }
      break;
      
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected - cable unplugged or link lost");
      logWarning("NETWORK", "Ethernet cable disconnected");
      ethernetConnected = false;
      
      // Trigger WiFi fallback if available
      if (!wifiConnected && config_ssid != "" && config_password != "") {
        Serial.println("Attempting WiFi fallback...");
        setupWifi();  // This will handle LED status and NTP sync
      } else {
        #ifdef USE_RGB_LED
        setStatusLED(STATUS_NETWORK_ERROR);
        #endif
      }
      break;
      
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      logError("NETWORK", "Ethernet hardware stopped");
      ethernetConnected = false;
      break;
      
    default:
      break;
  }
}

/**
 * FreeRTOS Task for Ethernet Monitoring (demo pattern)
 * Monitors connection state and handles reconnection
 */
void EthernetTask(void *parameter) {
  static bool eth_connected_old = false;
  
  while(1) {
    // Detect state transitions
    if (ethernetConnected && !eth_connected_old) {
      eth_connected_old = ethernetConnected;
      Serial.println("✅ Network port connected!");
      logInfo("NETWORK", "Ethernet fully operational");
      
      #ifdef USE_RGB_LED
      // Flash green to indicate successful connection
      for(int i = 0; i < 3; i++) {
        setStatusLED(STATUS_OFF);
        vTaskDelay(pdMS_TO_TICKS(100));
        setStatusLED(STATUS_ETHERNET_CONNECTED);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      #endif
    }
    else if (!ethernetConnected && eth_connected_old) {
      eth_connected_old = ethernetConnected;
      Serial.println("❌ Network port disconnected!");
      logWarning("NETWORK", "Ethernet connection lost");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms
  }
  vTaskDelete(NULL);
}

/**
 * Initialize Ethernet hardware (W5500 via SPI)
 * Returns true if initialization successful
 */
bool initEthernet() {
  Serial.println("\n=== Initializing Ethernet (W5500) ===");
  Serial.printf("Using pins: CS=%d, SCK=%d, MISO=%d, MOSI=%d, RST=%d\n",
                ETH_CS_PIN, ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_RST_PIN);
  
  // Register event handler FIRST (before ETH.begin)
  Network.onEvent(onEthernetEvent);
  
  // Initialize SPI bus for W5500
  SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN);
  
  // Initialize ETH class with W5500 parameters
  // ETH.begin(PHY_TYPE, PHY_ADDR, CS_PIN, IRQ_PIN, RST_PIN, SPI_BUS)
  bool success = ETH.begin(
    ETH_PHY_W5500,     // W5500 chip
    ETH_PHY_ADDR,      // PHY address (1)
    ETH_CS_PIN,        // Chip Select
    ETH_INT_PIN,       // Interrupt pin
    ETH_RST_PIN,       // Reset pin
    SPI                // SPI bus
  );
  
  if (!success) {
    Serial.println("❌ ERROR: ETH.begin() failed!");
    logCritical("NETWORK", "Ethernet initialization failed - check wiring");
    return false;
  }
  
  Serial.println("Ethernet hardware initialized");
  logInfo("NETWORK", "W5500 initialized, waiting for cable...");
  
  // Create FreeRTOS monitoring task (demo pattern)
  xTaskCreatePinnedToCore(
    EthernetTask,         // Task function
    "EthernetTask",       // Task name
    4096,                 // Stack size
    NULL,                 // Parameters
    2,                    // Priority
    &ethernetTaskHandle,  // Task handle
    0                     // Core 0
  );
  
  ethernetInitialized = true;
  return true;
}

/**
 * Setup Ethernet (called from main setup)
 * Waits for actual connection (IP address) before returning
 */
bool setupEthernet() {
  #ifdef USE_RGB_LED
  setStatusLED(STATUS_ETHERNET_CONNECTING);
  #endif
  
  if (!initEthernet()) {
    return false;  // Hardware init failed
  }
  
  // Wait up to 10 seconds for Ethernet to connect and get IP
  Serial.println("Waiting for Ethernet connection...");
  int attempts = 0;
  while (!ethernetConnected && attempts < 100) {
    delay(100);
    attempts++;
    if (attempts % 10 == 0) {
      Serial.print(".");
    }
  }
  
  if (ethernetConnected) {
    Serial.println(" connected!");
    return true;
  } else {
    Serial.println(" timeout");
    Serial.println("❌ Ethernet connection timeout (no cable or DHCP failed)");
    return false;
  }
}

/**
 * Maintain Ethernet Connection (called from main loop)
 * Note: With event-driven architecture, this is mostly passive
 * The FreeRTOS task and event handlers manage state automatically
 */
void maintainEthernet() {
  // With ETH class + events, no active maintenance needed
  // Connection state is updated automatically via onEthernetEvent()
  // The FreeRTOS EthernetTask monitors for state changes
  
  // Optional: DHCP lease renewal (ETH class handles this internally)
  // No explicit action required here
}

#endif // USE_ETHERNET
