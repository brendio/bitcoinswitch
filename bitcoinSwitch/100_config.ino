#define VERSION "v1.0.1"

#define BOOTUP_TIMEOUT 2 // seconds
#define CONFIG_FILE "/elements.json"

// uncomment if you dont want to use the configuration file
// #define HARDCODED

// device specific configuration / defaults
#define CONFIG_SSID "mywifi"
#define CONFIG_PASSWORD "mypw"
#define CONFIG_DEVICE_STRING ""
#define CONFIG_THRESHOLD_INKEY ""  // Invoice/read key of LNbits wallet
#define CONFIG_THRESHOLD_AMOUNT ""  // In sats
#define CONFIG_THRESHOLD_PIN ""  // GPIO pin
#define CONFIG_THRESHOLD_TIME "" // Time to turn pin on

#ifdef HARDCODED
void setupConfig(){
    Serial.println("Setting hardcoded values...");
    config_ssid = CONFIG_SSID;
    Serial.println("SSID: " + config_ssid);
    config_password = CONFIG_PASSWORD;
    Serial.println("SSID password: " + config_password);
    config_device_string = CONFIG_DEVICE_STRING;
    Serial.println("Device string: " + config_device_string);
    config_threshold_inkey = CONFIG_THRESHOLD_INKEY;
    Serial.println("Threshold inkey: " + config_threshold_inkey);
    String threshold_amount = String(CONFIG_THRESHOLD_AMOUNT);
    if (threshold_amount == "") {
        config_threshold_amount = 0;
    } else {
        config_threshold_amount = threshold_amount.toInt();
    }
    Serial.println("Threshold amount: " + String(config_threshold_amount));
    String led_pin = String(CONFIG_THRESHOLD_PIN);
    config_threshold_pin = led_pin.toInt();
    Serial.println("Threshold pin: " + String(config_threshold_pin));
    String led_time = String(CONFIG_THRESHOLD_TIME);
    config_threshold_time = led_time.toInt();
    Serial.println("Threshold time: " + String(config_threshold_time));
}
#else
#include <FS.h>
#include <SPIFFS.h>

void setupConfig(){
    SPIFFS.begin(true);
    // first give the installer a chance to delete configuration file
    executeConfigBoot();
    String fileContent = readConfig();
    // file does not exist, so we will enter endless config mode
    if (fileContent == "") {
        Serial.println("Config file does not exist.");
        executeConfigForever();
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, fileContent);
    if(error){
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        logError("CONFIG", "JSON parse error: " + String(error.c_str()));
    }

    config_ssid = getJsonValue(doc, "config_ssid", CONFIG_SSID);
    config_password = getJsonValue(doc, "config_password", CONFIG_PASSWORD);
    config_device_string = getJsonValue(doc, "config_device_string", CONFIG_DEVICE_STRING);
    config_threshold_inkey = getJsonValue(doc, "config_threshold_inkey", CONFIG_THRESHOLD_INKEY);
    String threshold_amount = getJsonValue(doc, "config_threshold_amount", CONFIG_THRESHOLD_AMOUNT);
    if (threshold_amount == "") {
        config_threshold_amount = 0;
    } else {
        config_threshold_amount = threshold_amount.toInt();
    }
    String led_pin = getJsonValue(doc, "config_threshold_pin", CONFIG_THRESHOLD_PIN);
    config_threshold_pin = led_pin.toInt();
    Serial.println("Threshold pin: " + String(config_threshold_pin));
    String led_time = getJsonValue(doc, "config_threshold_time", CONFIG_THRESHOLD_TIME);
    config_threshold_time = led_time.toInt();
    Serial.println("Threshold time: " + String(config_threshold_time));
    
    // Network configuration (optional)
    config_static_ip = getJsonValue(doc, "static_ip", "");
    config_static_gateway = getJsonValue(doc, "static_gateway", "");
    config_static_subnet = getJsonValue(doc, "static_subnet", "255.255.255.0");
    
    // Telegram configuration (optional)
    telegram_bot_token = getJsonValue(doc, "telegram_bot_token", "");
    telegram_chat_id = getJsonValue(doc, "telegram_chat_id", "");
    device_name = getJsonValue(doc, "device_name", "Waveshare-01");
    
    // DI monitoring configuration (optional)
    String di_enabled = getJsonValue(doc, "di_monitor_enabled", "false");
    di_monitor_enabled = (di_enabled == "true" || di_enabled == "1");
    String timeout_str = getJsonValue(doc, "di_check_timeout_ms", "2000");
    di_check_timeout_ms = timeout_str.toInt();
    
    // DI monitoring uses 1:1 mapping (Relay N → DI N) - no config needed
    
    // Logging configuration (optional)
    String log_enabled = getJsonValue(doc, "logging_enabled", "true");
    logging_enabled = (log_enabled == "true" || log_enabled == "1");
    String retention = getJsonValue(doc, "log_retention_hours", "168");
    log_retention_hours = retention.toInt();
    syslog_server = getJsonValue(doc, "syslog_server", "");
    String port_str = getJsonValue(doc, "syslog_port", "514");
    syslog_port = port_str.toInt();
}

String readConfig() {
    File paramFile = SPIFFS.open(CONFIG_FILE, FILE_READ);
    if (!paramFile) {
        return "";
    }
    String fileContent = paramFile.readString();
    if (fileContent == "") {
        return "";
    }
    paramFile.close();
    return fileContent;
}

String getJsonValue(JsonDocument &doc, const char* name, String defaultValue)
{
    String value = defaultValue;
    for (JsonObject elem : doc.as<JsonArray>()) {
        if (strcmp(elem["name"], name) == 0) {
            value = elem["value"].as<String>();
            Serial.println(String(name) + ": " + value);
            return value;
        }
    }
    Serial.println(String(name) + " (using default): " + value);
    return defaultValue;
}

void executeConfigBoot() {
    Serial.println("Entering boot mode. Waiting for " + String(BOOTUP_TIMEOUT) + " seconds.");
    clearTFT();
    printTFT("BOOT MODE", 21, 21);
    
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_BOOTING);
    #endif
    
    int counter = BOOTUP_TIMEOUT + 1;
    while (counter-- > 0) {
        if (Serial.available() == 0) {
            delay(1000);
            continue;
        }
        Serial.println();
        // if we get serial data in the first 5 seconds, we will enter config mode
        counter = 0;
        executeConfigForever();
    }
    Serial.println("Exiting boot mode.");
    Serial.print("Welcome to BitcoinSwitch!");
    Serial.println(" (" + String(VERSION) + ")");
    clearTFT();
    printTFT("BitcoinSwitch", 21, 21);
    printTFT(String(VERSION), 21, 42);
}

void executeConfigForever() {
    Serial.println("Entering config mode. until we receive /config-done.");
    clearTFT();
    printTFT("CONFIG", 21, 21);
    
    #ifdef USE_RGB_LED
    setStatusLED(STATUS_CONFIG_MODE);
    Serial.println("LED: Config mode (blue fast blink)");
    #endif
    
    bool done = false;
    while (true) {
        #ifdef USE_RGB_LED
        updateStatusLED();  // Keep LED animation running
        #endif
        
        done = executeConfig();
        if (done) {
            Serial.println("Exiting config mode.");
            return;
        }
    }
}

bool executeConfig() {
  if (Serial.available() == 0) return false;
  String data = Serial.readStringUntil('\n');
  Serial.println("received serial data: " + data);
  
  // Reset/reboot command
  if (data == "/reset" || data == "/reboot") {
    Serial.println("Rebooting device...");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
  
  // List files command (for config mode detection)
  if (data == "/file-list") {
    Serial.println("Config mode active - available commands:");
    Serial.println("  /file-list, /file-read, /file-append, /file-remove, /reset, /config-done");
    if (SPIFFS.exists(CONFIG_FILE)) {
      Serial.println("Config file: /elements.json exists");
    } else {
      Serial.println("Config file: /elements.json not found");
    }
    return false;
  }
  
  if (data == "/config-done") {
    delay(1000);
    return true;
  }
  if (data == "/file-remove") {
    SPIFFS.remove(CONFIG_FILE);
  }
  if (data.startsWith("/file-append")) {
    File file = SPIFFS.open(CONFIG_FILE, FILE_APPEND);
    if (!file) {
      file = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
    }
    if (!file) {
      Serial.println("Failed to open file for writing.");
    }
    if (file) {
      int pos = data.indexOf(" ");
      String jsondata = data.substring(pos + 1);
      file.println(jsondata);
      file.close();
    }
  }
  if (data.startsWith("/file-read")) {
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (file) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println("/file-send " + line);
      }
      file.close();
      Serial.println("/file-done");
    }
    return false;
  }
  return false;
}
#endif
