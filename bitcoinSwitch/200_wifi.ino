#include <WiFi.h>

void setupWifi() {
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_WIFI_CONNECTING);
    #endif
    
    WiFi.begin(config_ssid.c_str(), config_password.c_str());
    Serial.print("Connecting to WiFi.");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        digitalWrite(2, HIGH);
        Serial.print(".");
        delay(500);
        digitalWrite(2, LOW);
    }
    Serial.println();
    Serial.println("WiFi connection etablished!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_WIFI_CONNECTED);
    #endif
    
    printHome(true, false, false);
}

void loopWifi() {
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected!");
        printHome(false, false, false);
        
        #ifdef USE_RGB_LED
        setStatusLED(STATUS_NETWORK_ERROR);
        #endif
        
        delay(500);
        setupWifi();
    }
}
