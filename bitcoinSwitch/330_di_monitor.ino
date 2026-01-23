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
 */
void monitorDIAfterRelay(int relayNum, int duration_ms) {
  if (!di_monitor_enabled) {
    return;
  }
  
  // Get the DI pin mapped to this relay (relayNum is 1-based)
  int diPin = 0;
  if (relayNum >= 1 && relayNum <= 8) {
    diPin = di_relay_input_map[relayNum - 1];
  }
  
  if (diPin == 0) {
    Serial.printf("No DI monitoring configured for Relay %d\n", relayNum);
    return;
  }
  
  Serial.printf("DI monitoring enabled for Relay %d -> DI pin %d\n", relayNum, diPin);
  
  // Check if DI state changed
  bool success = checkDIStateChange(diPin, di_check_timeout_ms);
  
  if (success) {
    Serial.printf("✅ DI monitoring: Relay %d -> DI %d OK\n", relayNum, diPin);
    logInfo("DI_MONITOR", "Relay " + String(relayNum) + " -> DI " + String(diPin) + " OK");
    sendTelegramDISuccess(relayNum, diPin);
  } else {
    Serial.printf("❌ DI monitoring: Relay %d -> DI %d FAILED\n", relayNum, diPin);
    logError("DI_MONITOR", "Relay " + String(relayNum) + " -> DI " + String(diPin) + " FAILED - No state change");
    sendTelegramDIFailure(relayNum, diPin);
  }
}

#else
// Stub functions when not using Waveshare

bool checkDIStateChange(int diPin, int timeout_ms) { return true; }
void monitorDIAfterRelay(int relayNum, int duration_ms) {}

#endif
