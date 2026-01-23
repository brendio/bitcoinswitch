// ============================================
// Digital Input Monitoring for Waveshare ESP32-S3-ETH-8DI-8RO
// Monitors DI state changes after relay triggers
// ============================================

#ifdef WAVESHARE

/**
 * Check if a digital input changed state after relay trigger
 * Returns true if DI changed as expected, false if timeout/no change
 */
bool checkDIStateChange(int diPin, int timeout_ms) {
  if (diPin == 0) {
    // No DI mapped for this relay
    return true; // Consider it successful (skip check)
  }
  
  Serial.printf("Monitoring DI pin %d for state change (timeout %dms)...\n", diPin, timeout_ms);
  
  // Read initial state
  int initialState = digitalRead(diPin);
  Serial.printf("  Initial DI state: %d\n", initialState);
  
  // Wait for state change with timeout
  unsigned long startTime = millis();
  int currentState = initialState;
  
  while (millis() - startTime < timeout_ms) {
    currentState = digitalRead(diPin);
    
    if (currentState != initialState) {
      unsigned long elapsedTime = millis() - startTime;
      Serial.printf("✅ DI state changed: %d -> %d (after %lums)\n", 
                    initialState, currentState, elapsedTime);
      return true;
    }
    
    delay(10); // Small delay between reads
  }
  
  Serial.printf("⚠️ DI state did NOT change after %dms (still %d)\n", timeout_ms, currentState);
  return false;
}

/**
 * Monitor DI after relay trigger
 * Called from executePayment() after relay is activated
 * Uses 1:1 mapping: Relay 1 → DI 1, Relay 2 → DI 2, etc.
 */
void monitorDIAfterRelay(int relayNum, int duration_ms) {
  if (!di_monitor_enabled) {
    return;
  }
  
  // Waveshare hardware: 1:1 mapping (Relay N → DI N)
  // relayNum is 1-based (1-8), DI_PINS array is 0-based
  if (relayNum < 1 || relayNum > 8) {
    Serial.printf("Invalid relay number: %d\n", relayNum);
    return;
  }
  
  int diPin = DI_PINS[relayNum - 1];  // Direct mapping
  
  Serial.printf("DI monitoring: Relay %d -> DI %d (GPIO %d)\n", relayNum, relayNum, diPin);
  
  // Check if DI state changed
  bool success = checkDIStateChange(diPin, di_check_timeout_ms);
  
  if (success) {
    Serial.printf("✅ DI monitoring: Relay %d -> DI %d OK\n", relayNum, relayNum);
    logInfo("DI_MONITOR", "Relay " + String(relayNum) + " -> DI " + String(relayNum) + " OK");
    sendTelegramDISuccess(relayNum, relayNum);
  } else {
    Serial.printf("❌ DI monitoring: Relay %d -> DI %d FAILED\n", relayNum, relayNum);
    logError("DI_MONITOR", "Relay " + String(relayNum) + " -> DI " + String(relayNum) + " FAILED - No state change");
    sendTelegramDIFailure(relayNum, relayNum);
  }
}

#else
// Stub functions when not using Waveshare

bool checkDIStateChange(int diPin, int timeout_ms) { return true; }
void monitorDIAfterRelay(int relayNum, int duration_ms) {}

#endif
