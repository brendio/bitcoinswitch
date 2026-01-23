// ============================================
// Ethernet Module for Waveshare ESP32-S3-ETH-8DI-8RO
// W5500 Ethernet via SPI
// ============================================

#ifdef USE_ETHERNET

#include <Ethernet.h>
#include <SPI.h>
#include "device_config.h"

// Global Ethernet state (ethernetConnected declared in main sketch)
bool ethernetInitialized = false;
unsigned long lastEthernetCheck = 0;
const unsigned long ETHERNET_CHECK_INTERVAL = 5000; // Check every 5 seconds

// Use HSPI (SPI2) for Ethernet to avoid conflicts with default SPI
SPIClass SPI_Ethernet(HSPI);

// MAC address for device (customize per device if needed)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Static IP configuration (from config, optional)
IPAddress staticIP;
IPAddress staticGateway;
IPAddress staticSubnet(255, 255, 255, 0);
IPAddress staticDNS(8, 8, 8, 8);

/**
 * Initialize Ethernet hardware (W5500)
 * Returns true if hardware initialized (doesn't require link)
 */
bool initEthernet() {
  Serial.println("\n=== Initializing Ethernet (W5500) ===");
  
  // Reset W5500 if reset pin is connected
  if (ETH_RST_PIN > 0) {
    pinMode(ETH_RST_PIN, OUTPUT);
    digitalWrite(ETH_RST_PIN, LOW);
    delay(10);
    digitalWrite(ETH_RST_PIN, HIGH);
    delay(50);
  }
  
  // Initialize HSPI with correct pin mapping: SCK, MISO, MOSI, CS
  // NOTE: Pin order matters - manufacturer uses (12, 13, 11, 10) but our board uses different pins
  SPI_Ethernet.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);
  
  // Configure SPI settings for W5500
  SPI_Ethernet.setFrequency(20000000);  // 20MHz - manufacturer's setting
  SPI_Ethernet.setDataMode(SPI_MODE0);
  SPI_Ethernet.setBitOrder(MSBFIRST);
  
  // Initialize Ethernet library with CS pin
  Ethernet.init(ETH_CS_PIN);
  
  // Parse static IP config from strings if provided
  bool useStaticIP = false;
  if (config_static_ip != "" && config_static_ip != "0.0.0.0") {
    if (staticIP.fromString(config_static_ip)) {
      useStaticIP = true;
      Serial.println("Static IP configured: " + config_static_ip);
      
      // Parse gateway and subnet if provided
      if (config_static_gateway != "") {
        staticGateway.fromString(config_static_gateway);
      }
      if (config_static_subnet != "") {
        staticSubnet.fromString(config_static_subnet);
      }
    } else {
      Serial.println("Warning: Invalid static IP format, using DHCP");
    }
  }
  
  // Initialize Ethernet with HSPI
  Serial.print("Starting Ethernet on HSPI... ");
  
  if (useStaticIP) {
    Ethernet.begin(mac, staticIP, staticDNS, staticGateway, staticSubnet);
    Serial.println("(Static IP)");
  } else {
    Ethernet.begin(mac);
    Serial.println("(DHCP)");
  }
  
  // Give W5500 time to initialize
  delay(1000);
  
  ethernetInitialized = true;
  return true;
}

/**
 * Check Ethernet connection status
 * Returns true if Ethernet has an IP and link is up
 */
bool checkEthernetConnection() {
  if (!ethernetInitialized) {
    return false;
  }
  
  // Check link status
  auto linkStatus = Ethernet.linkStatus();
  
  if (linkStatus == LinkOFF) {
    if (ethernetConnected) {
      Serial.println("⚠️  Ethernet cable disconnected");
      ethernetConnected = false;
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_NETWORK_ERROR);
      #endif
    }
    return false;
  }
  
  if (linkStatus == LinkON) {
    // Check if we have an IP address
    IPAddress ip = Ethernet.localIP();
    
    if (ip[0] == 0) {
      // No IP yet (DHCP still negotiating)
      if (ethernetConnected) {
        Serial.println("⚠️  Ethernet link up but no IP address");
        ethernetConnected = false;
      }
      return false;
    }
    
    // We have link and IP
    if (!ethernetConnected) {
      Serial.println("✅ Ethernet connected!");
      Serial.print("   IP address: ");
      Serial.println(ip);
      Serial.print("   Subnet mask: ");
      Serial.println(Ethernet.subnetMask());
      Serial.print("   Gateway: ");
      Serial.println(Ethernet.gatewayIP());
      Serial.print("   DNS: ");
      Serial.println(Ethernet.dnsServerIP());
      
      logInfo("NETWORK", "Ethernet connected - IP: " + ip.toString());
      
      ethernetConnected = true;
      
      #ifdef USE_RGB_LED
      setStatusLED(STATUS_ETHERNET_CONNECTED);
      #endif
    }
    return true;
  }
  
  // Unknown link status
  return false;
}

/**
 * Setup Ethernet connection
 * Call this once at startup
 */
bool setupEthernet() {
  #ifdef USE_RGB_LED
  setStatusLED(STATUS_ETHERNET_CONNECTING);
  #endif
  
  if (!initEthernet()) {
    Serial.println("❌ Ethernet hardware initialization failed");
    return false;
  }
  
  // Wait up to 10 seconds for connection
  Serial.print("Waiting for Ethernet connection");
  for (int i = 0; i < 20; i++) {
    if (checkEthernetConnection()) {
      Serial.println(" connected!");
      return true;
    }
    Serial.print(".");
    delay(500);
  }
  
  Serial.println(" timeout");
  Serial.println("ℹ️  No Ethernet connection (cable may not be plugged in)");
  return false;
}

/**
 * Maintain Ethernet connection
 * Call this in loop() periodically
 */
void maintainEthernet() {
  if (!ethernetInitialized) {
    return;
  }
  
  // Check connection status periodically
  unsigned long now = millis();
  if (now - lastEthernetCheck >= ETHERNET_CHECK_INTERVAL) {
    lastEthernetCheck = now;
    
    // Maintain DHCP lease
    Ethernet.maintain();
    
    // Check if connection state changed
    checkEthernetConnection();
  }
}

#else
// Stub functions when Ethernet is not enabled

bool setupEthernet() {
  return false;
}

bool checkEthernetConnection() {
  return false;
}

void maintainEthernet() {
  // No-op
}

#endif // USE_ETHERNET
