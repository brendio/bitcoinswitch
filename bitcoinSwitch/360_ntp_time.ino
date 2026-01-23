// ============================================
// NTP Time Synchronization Module
// Syncs system time with NTP servers when network connects
// Based on Waveshare demo pattern
// ============================================

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

// NTP Client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // UTC, update every 60s

// Time sync state
bool timeIsSynced = false;
unsigned long lastTimeSyncAttempt = 0;
const unsigned long TIME_SYNC_RETRY_INTERVAL = 300000;  // Retry every 5 minutes if failed

// Timezone offset (in seconds)
// Default: UTC (0), Change for your timezone (e.g., UTC+8 = 28800)
int timezoneOffset = 0;  // Set to 0 for UTC, logs will show UTC time

/**
 * Synchronize time with NTP server
 * Should be called after network (Ethernet or WiFi) connects
 */
bool syncTimeWithNTP() {
  if (!ethernetConnected && !wifiConnected) {
    Serial.println("⚠️  No network connection for NTP sync");
    return false;
  }
  
  Serial.println("\n=== Syncing Time with NTP ===");
  Serial.println("NTP Server: pool.ntp.org");
  
  timeClient.begin();
  
  // Try to update time (with timeout)
  for (int attempt = 0; attempt < 5; attempt++) {
    if (timeClient.update()) {
      // Get epoch time from NTP
      time_t currentTime = timeClient.getEpochTime();
      
      // Validate timestamp (must be after Jan 1, 2021)
      if (currentTime < 1609459200) {
        Serial.printf("⚠️  Invalid timestamp received: %lu\n", currentTime);
        delay(1000);
        continue;
      }
      
      // Set system time
      struct timeval tv;
      tv.tv_sec = currentTime + timezoneOffset;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      
      // Get formatted time
      struct tm *localTime = localtime(&currentTime);
      char timeStr[64];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", localTime);
      
      Serial.println("✅ Time synchronized!");
      Serial.printf("   Current time: %s\n", timeStr);
      Serial.printf("   Unix timestamp: %lu\n", currentTime);
      
      logInfo("SYSTEM", String("Time synced via NTP: ") + timeStr);
      
      timeIsSynced = true;
      lastTimeSyncAttempt = millis();
      return true;
    }
    
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println(" timeout");
  Serial.println("❌ NTP sync failed - will retry later");
  logWarning("SYSTEM", "NTP sync failed - retrying in 5 minutes");
  
  lastTimeSyncAttempt = millis();
  return false;
}

/**
 * Check if time needs to be synced
 * Call this periodically from loop()
 */
void maintainTimeSync() {
  // Only sync if we have network and either:
  // 1. Time has never been synced, OR
  // 2. It's been 5+ minutes since last attempt
  if ((ethernetConnected || wifiConnected) && !timeIsSynced) {
    unsigned long now = millis();
    if (now - lastTimeSyncAttempt >= TIME_SYNC_RETRY_INTERVAL) {
      syncTimeWithNTP();
    }
  }
}

/**
 * Get current time as formatted string
 * Returns "YYYY-MM-DD HH:MM:SS" if synced, or uptime if not synced
 */
String getCurrentTimeString() {
  if (timeIsSynced) {
    time_t now = time(nullptr);
    struct tm *timeInfo = localtime(&now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    return String(buffer);
  } else {
    // Fall back to uptime if not synced
    unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "+%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buffer);
  }
}

/**
 * Get Unix timestamp (seconds since epoch)
 * Returns 0 if time not synced
 */
time_t getCurrentTimestamp() {
  if (timeIsSynced) {
    return time(nullptr);
  }
  return 0;
}
