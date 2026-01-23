// ============================================
// I2C Relay Controller for Waveshare ESP32-S3-ETH-8DI-8RO
// Controls 8 relays via TCA9554PWR I2C GPIO Expander
// With demo-style error handling and failure detection
// ============================================

#ifdef WAVESHARE
#include <Wire.h>
#include "device_config.h"

// Demo-style failure tracking
bool relayFailureFlag = false;
TaskHandle_t relayFailTaskHandle = NULL;

// I2C Relay Controller Class
class RelayControllerI2C {
private:
    uint8_t currentState;  // Track current relay states (bit mask)
    bool initialized;

    // Read a register from the TCA9554
    uint8_t readRegister(uint8_t reg) {
        Wire.beginTransmission(TCA9554_ADDRESS);
        Wire.write(reg);
        Wire.endTransmission();
        Wire.requestFrom(TCA9554_ADDRESS, (uint8_t)1);
        if (Wire.available()) {
            return Wire.read();
        }
        return 0;
    }

    // Write a register to the TCA9554
    bool writeRegister(uint8_t reg, uint8_t value) {
        Wire.beginTransmission(TCA9554_ADDRESS);
        Wire.write(reg);
        Wire.write(value);
        return (Wire.endTransmission() == 0);
    }

public:
    RelayControllerI2C() : currentState(0), initialized(false) {}

    // Initialize I2C and configure TCA9554 for output mode
    bool begin() {
        Serial.println("Initializing I2C Relay Controller...");
        
        // Initialize I2C bus
        Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
        delay(100);

        // Check if TCA9554 is responding
        Wire.beginTransmission(TCA9554_ADDRESS);
        uint8_t error = Wire.endTransmission();
        if (error != 0) {
            Serial.println("ERROR: TCA9554 not found at address 0x20!");
            Serial.printf("I2C error code: %d\n", error);
            logCritical("RELAY", "TCA9554 not found - relay control unavailable (I2C error: " + String(error) + ")");
            return false;
        }
        Serial.println("TCA9554 I2C expander found");

        // Configure all pins as outputs (0 = output, 1 = input)
        if (!writeRegister(TCA9554_CONFIG_REG, 0x00)) {
            Serial.println("ERROR: Failed to configure TCA9554 pins as outputs");
            return false;
        }

        // Turn all relays OFF initially
        currentState = RELAY_ACTIVE_HIGH ? 0x00 : 0xFF;
        if (!writeRegister(TCA9554_OUTPUT_REG, currentState)) {
            Serial.println("ERROR: Failed to initialize relay states");
            return false;
        }

        Serial.println("Relay controller initialized - all relays OFF");
        initialized = true;
        return true;
    }

    // Set a specific relay state (relayNum: 0-7 for Relay 1-8)
    bool setRelay(uint8_t relayNum, bool state) {
        if (!initialized) {
            Serial.println("ERROR: Relay controller not initialized!");
            relayFailureFlag = true;  // Demo-style error flag
            return false;
        }

        if (relayNum >= RELAY_COUNT) {
            Serial.printf("ERROR: Invalid relay number %d (valid: 0-%d)\n", relayNum, RELAY_COUNT - 1);
            relayFailureFlag = true;  // Demo-style error flag
            return false;
        }

        // Update the bit for this relay
        if (state) {
            if (RELAY_ACTIVE_HIGH) {
                currentState |= (1 << relayNum);   // Set bit high
            } else {
                currentState &= ~(1 << relayNum);  // Set bit low
            }
        } else {
            if (RELAY_ACTIVE_HIGH) {
                currentState &= ~(1 << relayNum);  // Set bit low
            } else {
                currentState |= (1 << relayNum);   // Set bit high
            }
        }

        // Write the new state to the I2C expander
        if (!writeRegister(TCA9554_OUTPUT_REG, currentState)) {
            Serial.printf("ERROR: Failed to set relay %d\n", relayNum + 1);
            relayFailureFlag = true;  // Demo-style error flag
            logError("RELAY", "Failed to set relay " + String(relayNum + 1));
            return false;
        }

        Serial.printf("Relay %d: %s\n", relayNum + 1, state ? "ON" : "OFF");
        return true;
    }

    // Trigger a relay for a specific duration (milliseconds)
    bool triggerRelay(uint8_t relayNum, unsigned long duration_ms) {
        if (!setRelay(relayNum, true)) {
            relayFailureFlag = true;  // Demo-style error flag
            return false;
        }
        delay(duration_ms);
        return setRelay(relayNum, false);
    }

    // Turn all relays OFF
    bool allOff() {
        if (!initialized) return false;
        
        currentState = RELAY_ACTIVE_HIGH ? 0x00 : 0xFF;
        if (!writeRegister(TCA9554_OUTPUT_REG, currentState)) {
            Serial.println("ERROR: Failed to turn all relays OFF");
            return false;
        }
        Serial.println("All relays OFF");
        return true;
    }

    // Get current state of a specific relay
    bool getRelayState(uint8_t relayNum) {
        if (relayNum >= RELAY_COUNT) return false;
        
        bool bitState = (currentState & (1 << relayNum)) != 0;
        return RELAY_ACTIVE_HIGH ? bitState : !bitState;
    }

    // Get the current state byte (for debugging)
    uint8_t getCurrentState() {
        return currentState;
    }

    // Test all relays in sequence
    void testSequence(unsigned long duration_ms = 500) {
        Serial.println("Testing all relays...");
        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            Serial.printf("Testing Relay %d\n", i + 1);
            triggerRelay(i, duration_ms);
            delay(200);  // Small delay between tests
        }
        Serial.println("Relay test complete");
    }
};

// Global relay controller instance
RelayControllerI2C relayController;

/**
 * Relay Failure Monitoring Task (demo pattern)
 * Monitors relay failures and provides visual/audio feedback
 */
void RelayFailTask(void *parameter) {
    while(1) {
        if (relayFailureFlag) {
            relayFailureFlag = false;  // Clear flag
            
            Serial.println("⚠️  Relay control failure detected!");
            logError("RELAY", "Relay control failed - check I2C connection");
            
            // Visual feedback via RGB LED (red flash)
            #ifdef USE_RGB_LED
            for (int i = 0; i < 5; i++) {
                setStatusLED(STATUS_ERROR);
                vTaskDelay(pdMS_TO_TICKS(500));
                setStatusLED(STATUS_OFF);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            // Restore previous state
            setStatusLED(STATUS_ETHERNET_CONNECTED);
            #endif
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Check every 50ms
    }
    vTaskDelete(NULL);
}

// Helper functions for backward compatibility with GPIO-based code
bool initRelayController() {
    bool success = relayController.begin();
    
    if (success) {
        // Create FreeRTOS monitoring task (demo pattern)
        xTaskCreatePinnedToCore(
            RelayFailTask,           // Task function
            "RelayFailTask",         // Task name
            4096,                    // Stack size
            NULL,                    // Parameters
            3,                       // Priority (higher than Ethernet)
            &relayFailTaskHandle,    // Task handle
            0                        // Core 0
        );
        Serial.println("Relay failure monitoring task started");
    }
    
    return success;
}

bool setRelayPin(uint8_t relayNum, bool state) {
    return relayController.setRelay(relayNum, state);
}

bool triggerRelay(uint8_t relayNum, unsigned long duration_ms) {
    return relayController.triggerRelay(relayNum, duration_ms);
}

#endif // WAVESHARE
