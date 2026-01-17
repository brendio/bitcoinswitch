// ============================================
// I2C Relay Controller for Waveshare ESP32-S3-ETH-8DI-8RO
// Controls 8 relays via TCA9554PWR I2C GPIO Expander
// ============================================

#ifdef WAVESHARE
#include <Wire.h>
#include "device_config.h"

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
            return false;
        }

        if (relayNum >= RELAY_COUNT) {
            Serial.printf("ERROR: Invalid relay number %d (valid: 0-%d)\n", relayNum, RELAY_COUNT - 1);
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
            return false;
        }

        Serial.printf("Relay %d: %s\n", relayNum + 1, state ? "ON" : "OFF");
        return true;
    }

    // Trigger a relay for a specific duration (milliseconds)
    bool triggerRelay(uint8_t relayNum, unsigned long duration_ms) {
        if (!setRelay(relayNum, true)) {
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

// Helper functions for backward compatibility with GPIO-based code
bool initRelayController() {
    return relayController.begin();
}

bool setRelayPin(uint8_t relayNum, bool state) {
    return relayController.setRelay(relayNum, state);
}

bool triggerRelay(uint8_t relayNum, unsigned long duration_ms) {
    return relayController.triggerRelay(relayNum, duration_ms);
}

#endif // WAVESHARE
