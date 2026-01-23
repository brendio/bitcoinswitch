// ============================================
// Event Logging System for Waveshare ESP32-S3-ETH-8DI-8RO
// Hybrid approach: SPIFFS persistent log + RAM circular buffer
// ============================================

#include <SPIFFS.h>

// Log file path
#define LOG_FILE "/events.log"
#define MAX_LOG_SIZE 180000  // ~180KB max log size (leave room for other SPIFFS data)

// RAM circular buffer for recent events
#define RAM_LOG_SIZE 100
String ramLogBuffer[RAM_LOG_SIZE];
int ramLogIndex = 0;
int ramLogCount = 0;

// LogLevel enum defined in main sketch

// Timestamp tracking (uptime in seconds)
unsigned long bootTime = 0;

/**
 * Initialize logging system
 */
void initLogging() {
  bootTime = millis();
  
  if (!logging_enabled) {
    Serial.println("Logging disabled in configuration");
    return;
  }
  
  Serial.println("\n=== Initializing Logging System ===");
  
  // Check if SPIFFS is mounted
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: SPIFFS mount failed!");
    logging_enabled = false;
    return;
  }
  
  // Check log file size
  if (SPIFFS.exists(LOG_FILE)) {
    File logFile = SPIFFS.open(LOG_FILE, "r");
    size_t fileSize = logFile.size();
    logFile.close();
    
    Serial.printf("Existing log file: %d bytes\n", fileSize);
    
    // Rotate if too large
    if (fileSize > MAX_LOG_SIZE) {
      Serial.println("Log file too large, rotating...");
      rotateLog();
    }
  } else {
    Serial.println("Creating new log file...");
  }
  
  // Log startup
  logEvent(LOG_INFO, "SYSTEM", "Device started");
  Serial.println("Logging system initialized");
}

/**
 * Rotate log file (move to .old and start fresh)
 */
void rotateLog() {
  if (!logging_enabled) return;
  
  // Remove old backup if exists
  if (SPIFFS.exists("/events.log.old")) {
    SPIFFS.remove("/events.log.old");
  }
  
  // Rename current log to .old
  if (SPIFFS.exists(LOG_FILE)) {
    SPIFFS.rename(LOG_FILE, "/events.log.old");
  }
  
  Serial.println("Log rotated");
}

/**
 * Get formatted timestamp string (uptime in HH:MM:SS)
 */
String getLogTimestamp() {
  unsigned long uptime = (millis() - bootTime) / 1000;
  int hours = uptime / 3600;
  int minutes = (uptime % 3600) / 60;
  int seconds = uptime % 60;
  
  char timestamp[16];
  sprintf(timestamp, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(timestamp);
}

/**
 * Get log level string
 */
String getLogLevelString(LogLevel level) {
  switch (level) {
    case LOG_INFO:     return "INFO";
    case LOG_WARNING:  return "WARN";
    case LOG_ERROR:    return "ERROR";
    case LOG_CRITICAL: return "CRIT";
    default:           return "UNKN";
  }
}

/**
 * Log an event to both SPIFFS and RAM buffer
 */
void logEvent(LogLevel level, const char* category, String message) {
  if (!logging_enabled) return;
  
  // Format: TIMESTAMP | LEVEL | CATEGORY | MESSAGE
  String logEntry = getLogTimestamp() + " | " + 
                    getLogLevelString(level) + " | " +
                    String(category) + " | " + 
                    message;
  
  // Add to RAM circular buffer
  ramLogBuffer[ramLogIndex] = logEntry;
  ramLogIndex = (ramLogIndex + 1) % RAM_LOG_SIZE;
  if (ramLogCount < RAM_LOG_SIZE) {
    ramLogCount++;
  }
  
  // Write to SPIFFS
  File logFile = SPIFFS.open(LOG_FILE, "a");
  if (logFile) {
    logFile.println(logEntry);
    logFile.close();
    
    // Check if rotation needed
    logFile = SPIFFS.open(LOG_FILE, "r");
    if (logFile.size() > MAX_LOG_SIZE) {
      logFile.close();
      rotateLog();
    } else {
      logFile.close();
    }
  }
  
  // Also print to serial for debugging
  if (level >= LOG_WARNING) {
    Serial.println("[LOG] " + logEntry);
  }
}

/**
 * Convenience logging functions
 */
void logInfo(const char* category, String message) {
  logEvent(LOG_INFO, category, message);
}

void logWarning(const char* category, String message) {
  logEvent(LOG_WARNING, category, message);
}

void logError(const char* category, String message) {
  logEvent(LOG_ERROR, category, message);
}

void logCritical(const char* category, String message) {
  logEvent(LOG_CRITICAL, category, message);
}

/**
 * Get recent logs from RAM buffer
 */
String getRecentLogs(int count) {
  if (count > ramLogCount) {
    count = ramLogCount;
  }
  
  String result = "=== Recent Events (" + String(count) + ") ===\n";
  
  // Calculate starting index for circular buffer
  int startIdx = (ramLogIndex - count + RAM_LOG_SIZE) % RAM_LOG_SIZE;
  
  for (int i = 0; i < count; i++) {
    int idx = (startIdx + i) % RAM_LOG_SIZE;
    result += ramLogBuffer[idx] + "\n";
  }
  
  return result;
}

/**
 * Get full log from SPIFFS
 */
String getFullLog() {
  if (!logging_enabled || !SPIFFS.exists(LOG_FILE)) {
    return "No log file found";
  }
  
  File logFile = SPIFFS.open(LOG_FILE, "r");
  if (!logFile) {
    return "Error opening log file";
  }
  
  String content = logFile.readString();
  logFile.close();
  
  return content;
}

/**
 * Send log via syslog (UDP)
 */
void sendSyslog(LogLevel level, const char* category, String message) {
  if (syslog_server == "" || !wifiConnected) {
    return; // Syslog not configured or no network
  }
  
  // TODO: Implement syslog UDP packet
  // Priority = facility * 8 + severity
  // Facility 16 = local use 0
  // Severity: 0=emerg, 3=error, 4=warning, 6=info
  
  int severity;
  switch (level) {
    case LOG_CRITICAL: severity = 0; break;
    case LOG_ERROR:    severity = 3; break;
    case LOG_WARNING:  severity = 4; break;
    default:           severity = 6; break;
  }
  
  int priority = 16 * 8 + severity; // Local0 facility
  
  String syslogMsg = "<" + String(priority) + "> " + 
                     device_name + " " + 
                     String(category) + ": " + 
                     message;
  
  // Send UDP packet (implementation left for future)
  Serial.println("Syslog: " + syslogMsg);
}

/**
 * Cleanup old log entries based on retention policy
 */
void cleanupOldLogs() {
  // For now, just use file rotation
  // Could be enhanced to parse timestamps and delete old entries
}
