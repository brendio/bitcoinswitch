#include <WiFi.h>

// WiFi module (wifiConnected declared in main sketch)

bool setupWifi() {
    // Check if WiFi is enabled (credentials provided)
    if (config_ssid == "" || config_password == "") {
        Serial.println("WiFi disabled (no credentials configured)");
        return false;
    }
    
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_WIFI_CONNECTING);
    #endif
    
    WiFi.begin(config_ssid.c_str(), config_password.c_str());
    Serial.print("Connecting to WiFi");
    
    // Wait up to 20 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        Serial.print(".");
        delay(500);
        digitalWrite(2, !digitalRead(2)); // Blink onboard LED
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" timeout");
        Serial.println("❌ WiFi connection failed");
        wifiConnected = false;
        return false;
    }
    
    Serial.println(" connected!");
    Serial.println("✅ WiFi connection established!");
    Serial.print("   IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("   Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    wifiConnected = true;
    
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_WIFI_CONNECTED);
    #endif
    
    printHome(true, false, false);
    return true;
}

void loopWifi() {
    // Only monitor WiFi if it's the active connection
    // (Don't monitor if Ethernet is connected)
    #ifdef USE_ETHERNET
    if (ethernetConnected) {
        return; // Ethernet is active, don't manage WiFi
    }
    #endif
    
    // Check WiFi connection status
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  WiFi disconnected!");
        printHome(false, false, false);
        
        wifiConnected = false;
        
        #ifdef USE_RGB_LED
        setStatusLED(STATUS_NETWORK_ERROR);
        #endif
        
        // Try to reconnect
        delay(1000);
        Serial.println("Attempting to reconnect WiFi...");
        setupWifi();
    }
}
