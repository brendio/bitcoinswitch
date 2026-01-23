# bitcoinSwitch - AI Agent Instructions

## Project Overview
**bitcoinSwitch** (aka "Clicky") is an ESP32-based IoT relay controller that activates physical devices via Lightning Network payments. Users pay via Lightning (using LNbits LNURLdevice extension), and the device receives commands over WebSockets to trigger GPIO/relay outputs.

**Current Focus**: Adapting the codebase for Waveshare ESP32-S3-ETH-8DI-8RO hardware with:
- I2C relay control (TCA9554PWR expander vs direct GPIO)
- **Ethernet-first networking** (W5500 on SPI) with optional WiFi fallback
- RGB LED status indicators (WS2812) replacing TFT display

**Hardware Status**: Physical Waveshare device available for USB testing.

## Architecture & Data Flow

### Core Components
1. **Main Loop** ([bitcoinSwitch.ino](bitcoinSwitch/bitcoinSwitch.ino)): WebSocket client that connects to LNbits server
2. **Configuration** ([100_config.ino](bitcoinSwitch/100_config.ino)): SPIFFS-based config storage with serial protocol
3. **WiFi Management** ([200_wifi.ino](bitcoinSwitch/200_wifi.ino)): Connection/reconnection logic
4. **Display** ([300_tft.ino](bitcoinSwitch/300_tft.ino)): TFT display feedback (conditional compilation)

### Network Architecture (Waveshare-Specific)
**Priority**: Ethernet → WiFi (optional fallback) → Error State

```
W5500 Ethernet (SPI) ─┬─> LNbits WSS Connection
                      │
WiFi Fallback ────────┘   (configurable, can be disabled)
```

**Key Requirements**:
1. Attempt Ethernet connection first (W5500 on CS GPIO 10)
2. WiFi fallback only if Ethernet fails AND fallback enabled
3. **Do not continuously retry WiFi** if Ethernet re-establishes (causes slowdowns on certain networks)
4. Use RGB LED to indicate network state

**WiFi Fallback Strategy**: Check for blank credentials to disable fallback
```cpp
bool wifiEnabled = (config_ssid != "" && config_password != "");
if (!ethernetConnected && wifiEnabled) {
    setupWifi();
}
```
- If WiFi SSID/password left blank in config, WiFi fallback is disabled
- No additional config flag needed - simpler UX and fail-safe behavior

### WebSocket Payment Flow
```
LNbits Server → WSS Connection → ESP32 → Parse "{pin-time-comment}" → Trigger GPIO/Relay + LED Flash
```
- **Normal Mode**: WebSocket payload format is `{pin-time-comment}` (e.g., "12-5000-test")
- **Threshold Mode**: Disabled in v1.0+ (commented code retained for future use)
- Payment triggers `executePayment()` which sets I2C relay HIGH for specified duration

### Configuration System
- **Boot Mode**: On startup, device waits 2 seconds for serial input to enter config mode
- **Serial Protocol**: Commands like `/file-append`, `/file-read`, `/file-remove`, `/config-done`
- **Config File**: `/elements.json` stored in SPIFFS with WiFi credentials + LNbits device URL
- **Web Installer**: Browser-based flashing tool uploads firmware + writes config via serial

### Configuration Workflow

**Recommended Method**: Python configuration script (`configure_device.py`)

#### Setup (One-Time)
```bash
# Install required Python library
pip3 install pyserial

# Edit configuration template
nano config_template.json
# or
open config_template.json
```

#### Configure Device (3 Steps)
1. **Ensure device is in config mode** (blue fast blink LED)
   - Happens automatically if no config exists
   - Or send any serial input within 2 seconds of boot

2. **Run configuration script**
   ```bash
   python3 configure_device.py [/dev/cu.usbmodem101]
   ```
   - Script validates config before writing
   - Displays summary and asks for confirmation
   - Writes to device and verifies

3. **Device reboots automatically** and connects to network
   - Monitor connection with: `arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200`

#### Config Template Structure
```json
{
  "_comment": "Comments start with underscore (ignored by device)",
  
  "config_ssid": "YourWiFi",
  "config_password": "YourPassword",
  "config_device_string": "wss://lnbits.com/bitcoinswitch/api/v1/ws/DEVICE_ID",
  
  "telegram_bot_token": "",  // Optional: Leave blank to disable
  "telegram_chat_id": "",
  "device_name": "Waveshare-01",
  
  "static_ip": "",  // Optional: Blank = DHCP (recommended)
  "static_gateway": "",
  "static_subnet": "255.255.255.0"
}
```

#### Alternative Configuration Methods

| Method | Use Case | Pros | Cons |
|--------|----------|------|------|
| **Python Script** (Recommended) | Regular use, multiple devices | Validates config, fast, repeatable | Requires Python + pyserial |
| **Manual Serial** | Emergency/debugging | No dependencies | Tedious, error-prone, no validation |
| **Hardcoded Mode** | Development testing | No config file needed | Requires recompile for changes |
| **Web Installer** | End-user deployment | Best UX, browser-based | Not yet implemented |

**Manual Serial Example**:
```bash
# In arduino-cli monitor or screen:
/file-remove /elements.json
/file-append /elements.json [
/file-append /elements.json {"name":"config_ssid","value":"WiFiName"},
/file-append /elements.json {"name":"config_device_string","value":"wss://..."}
/file-append /elements.json ]
/config-done
```

**Hardcoded Mode**: Uncomment `#define HARDCODED` in [100_config.ino](bitcoinSwitch/100_config.ino), edit values, recompile

#### Configuration Parameters

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `config_device_string` | ✅ Yes | - | WebSocket URL to LNbits server (must start with `wss://`) |
| `config_ssid` | No | `""` | WiFi SSID (blank = Ethernet only, no WiFi fallback) |
| `config_password` | No | `""` | WiFi password |
| `device_name` | No | `"Waveshare-01"` | Human-readable device identifier (used in Telegram alerts) |
| `telegram_bot_token` | No | `""` | Telegram Bot API token (requires `chat_id` if set) |
| `telegram_chat_id` | No | `""` | Telegram chat ID for alerts (requires `bot_token` if set) |
| `static_ip` | No | `""` | Static IP address (blank = DHCP auto-config) |
| `static_gateway` | No | `""` | Gateway IP address (for static IP config) |
| `static_subnet` | No | `"255.255.255.0"` | Subnet mask (for static IP config) |
| `config_threshold_*` | No | `""` | Legacy threshold mode parameters (disabled in v1.0+) |

#### Configuration Design Principles

1. **Blank = Disabled**: Empty string values disable optional features (WiFi, Telegram, static IP). Device uses safe defaults.

2. **Backward Compatible**: New parameters added with safe defaults. Old config files continue to work without modification.

3. **Fail-Safe Defaults**: 
   - No WiFi credentials = Ethernet-only mode
   - No static IP = DHCP auto-configuration
   - No Telegram = silent operation

4. **Validation Before Write**: Python script checks:
   - `wss://` URL format
   - Required parameter pairs (bot_token ↔ chat_id)
   - IP address format
   - Placeholder values not committed

5. **Version Control Friendly**: 
   - `config_template.json` committed with placeholders
   - Actual credentials in `config_local.json` (gitignored, optional)
   - No sensitive data in repository

## Hardware Adaptation (Waveshare)

### Critical Differences
| Aspect | Original ESP32 | Waveshare ESP32-S3-ETH-8DI-8RO |
|--------|---------------|--------------------------------|
| **Relay Control** | Direct GPIO (`digitalWrite()`) | TCA9554PWR I2C expander (addr 0x20) |
| **Display** | Optional TFT via SPI | **NO DISPLAY** - headless operation |
| **Status Feedback** | TFT screen | WS2812 RGB LED (GPIO 38) |
| **Network** | WiFi only | **Ethernet primary** (W5500 SPI, CS GPIO 10) + WiFi fallback |
| **I2C Pins** | N/A | GPIO 42 (SDA), GPIO 41 (SCL) |
| **USB** | GPIO pins | USB CDC on boot |

### Adapter Pattern Required
Replace direct GPIO control in `executePayment()`:
```cpp
// Original
pinMode(pin, OUTPUT);
digitalWrite(pin, HIGH);

// Waveshare (needs I2C expander code)
setRelayPin(pin, true);  // Write to TCA9554 register 0x01
```

### RGB LED Status Patterns (WS2812 on GPIO 38)
Replace TFT display feedback with LED state machine:

```
Boot (Blue pulse)
  ↓
Ethernet attempt
  ↓
├─ Success → Solid Green
├─ Fail + WiFi enabled → WiFi attempt
│    ↓
│    ├─ Success → Solid Yellow  
│    └─ Fail → Solid Red (error)
└─ Fail + no WiFi → Solid Red
  ↓
WebSocket connection attempt
  ↓
├─ Success → Green pulse (replaces solid green/yellow)
└─ Fail → Revert to network state (solid green/yellow) + retry
  ↓
At rest: Green pulse = ready to receive payments
  ↓
Payment received → White flash (2x) → Return to green pulse
```

**LED Pattern Reference**:

| State | LED Pattern | Description |
|-------|-------------|-------------|
| **Boot Mode** | Blue pulse | Device startup |
| **Config Mode** | Blue fast blink | Serial config active |
| **Ethernet OK** | Solid Green | W5500 link (before WSS) |
| **WiFi OK** | Solid Yellow | WiFi fallback active (before WSS) |
| **Network Error** | Solid Red | No connection available |
| **Ready (WSS Connected)** | Green pulse | WebSocket to LNbits active |
| **Payment Trigger** | White flash (2x) | Relay activated |
| **WebSocket Lost** | Revert to Solid Green/Yellow | Shows underlying network still connected |

**Key Behavior**: WebSocket state (green pulse) takes over once WSS is established. If WebSocket drops, LED reverts to showing network layer status.

**Implementation Note**: Use FastLED or Adafruit_NeoPixel library for WS2812 control.

See [WAVESHARE_ADAPTER_GUIDE.md](WAVESHARE_ADAPTER_GUIDE.md) and [WAVESHARE_REBUILD_INSTRUCTIONS.md](WAVESHARE_REBUILD_INSTRUCTIONS.md) for detailed hardware specs and implementation phases.

## Build System

### Standard Build (arduino-cli)
```bash
# Build for specific device (esp32 or tdisplay)
./build.sh esp32

# Build with TFT support (reads tft_config_tdisplay.txt)
./build.sh tdisplay

# Debug: build + upload + monitor
./debug.sh /dev/ttyUSB0 esp32
```

**Key Build Properties**:
- Defines device macro: `-DESP32` or `-DTDISPLAY` or `-DWAVESHARE`
- TFT config injected via `-DTFT=1` + pin defines from `tft_config_*.txt` (not used for Waveshare)
- Ethernet config: `-DUSE_ETHERNET=1 -DETH_CLK_MODE=ETH_CLOCK_GPIO0_IN` for W5500
- Partitions: `min_spiffs` scheme (1966080 bytes max)
- FQBN: `esp32:esp32:esp32s3` (for Waveshare ESP32-S3)

### Web Installer Build
```bash
./build-webinstaller.sh
```
- Clones `lnbits/hardware-installer` template
- Downloads release binaries from GitHub (bootloader, partitions, main)
- Packages into browser-flashable installer at [bitcoinswitch.lnbits.com](https://bitcoinswitch.lnbits.com/)

## Code Conventions

### Multi-File Arduino Structure
Arduino IDE concatenates `.ino` files in **alphabetical order**:
1. `bitcoinSwitch.ino` - globals + setup/loop
2. `100_config.ino` - runs first due to numbering
3. `101_split_string.ino` - utility function
4. `200_wifi.ino`, `300_tft.ino` - subsystems

**Critical**: Function declarations not needed between `.ino` files (Arduino auto-generates prototypes).

### Conditional Compilation
```cpp
#ifdef TFT
  setupTFT();
  printTFT("message", x, y);
#endif

#ifdef USE_RGB_LED
  setLEDColor(0, 255, 0);  // Green for Waveshare
#endif

#ifdef USE_ETHERNET
  if (Ethernet.linkStatus() == LinkON) {
    // Ethernet connected
  }
#endif
```
- Use `#ifdef TFT` for display code (Waveshare has no display)
- Use `#ifdef USE_RGB_LED` for WS2812 status feedback (Waveshare only)
- Use `#ifdef USE_ETHERNET` for W5500 Ethernet code (Waveshare only)
- Use `#ifdef HARDCODED` to bypass SPIFFS config (testing only)

### Serial Communication Pattern
- Boot mode: Wait 2 sec for serial input, else proceed to normal operation
- Config mode: JSON config written line-by-line via `/file-append` commands
- Always use `Serial.println()` for debugging (USB CDC on ESP32-S3)

## Development Workflows

### Local Testing (Waveshare Hardware Available)
1. **Connect Hardware**: Attach Waveshare via USB-C (appears as `/dev/ttyUSB0`, `/dev/ttyACM0`, or `/dev/cu.usbmodem101` on macOS)
2. **Hardcoded Mode**: Edit `#define CONFIG_*` values in `100_config.ino`, uncomment `#define HARDCODED`
3. **Compile → Upload → Monitor Workflow** (see detailed section below)
4. **Monitor**: Serial output shows:
   - Ethernet link status
   - WiFi fallback state (if enabled)
   - WebSocket connection
   - Payment execution + relay triggers
5. **Verify RGB LED**: Check color patterns match expected state (see RGB LED Status Patterns above)
6. **Test Ethernet**: Connect/disconnect Ethernet cable to verify fallback behavior

### Compile → Upload → Monitor Workflow
**CRITICAL**: Each step takes time. **ALWAYS wait for complete output** before proceeding.

#### Step 1: Compile (30-60 seconds)
```bash
./build.sh waveshare
# OR for ESP32-S3 with USB CDC:
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
  --build-path build bitcoinSwitch
```
**Success indicators**:
- Output ends with `"Used library"` listings
- Final line shows sketch size: `"Sketch uses X bytes"` 
- No `"Error:"` or `"Failed"` messages
- **Wait for command to fully complete** - do NOT assume failure if output is slow

#### Step 2: Upload (10-20 seconds)
```bash
arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
  --port /dev/cu.usbmodem101 --input-dir build
```
**Success indicators**:
- `"Connecting..."` followed by dots
- `"Writing at 0x..."` progress messages
- Final: `"Hash of data verified."` and `"Leaving... Hard resetting via RTS pin..."`
- **Wait for complete upload** - takes 15-20 seconds for full firmware

**Common issues**:
- `"serial port not found"`: Device not connected or wrong port
- `"Failed to connect"`: Try pressing BOOT button during upload
- Agent must **wait for full output** before declaring success/failure

#### Step 3: Monitor Serial (continuous)
```bash
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```
**Expected output** (first 5 seconds after upload):
1. ESP32 boot messages
2. `"Setting hardcoded values..."` (if HARDCODED defined)
3. Network connection attempts
4. `"WiFi connected!"` or Ethernet status
5. `"Connecting to websocket: ..."`

**Monitor workflow**:
- Start monitor AFTER upload completes
- Let run for at least 10-15 seconds to see boot sequence
- Press Ctrl+C to stop monitoring (not a failure!)

#### Chained Command (Recommended)
```bash
./build.sh waveshare && \
  arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
    --port /dev/cu.usbmodem101 --input-dir build && \
  sleep 2 && \
  arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```
**Using `&&` ensures each step completes before next starts**

**AGENT INSTRUCTIONS**:
- **DO NOT assume failure** if compile/upload takes 30+ seconds
- **WAIT for terminal command to finish** (`isBackground=false`)
- **Read full output** before checking for errors
- **Look for success indicators** listed above
- **If monitoring, let it run 10-15 seconds** before analyzing output

### Adding New Device Support
1. Add device name to [versions.json](versions.json) `devices` array
2. Create `tft_config_<device>.txt` if device has display
3. Update `build.sh` to handle device-specific defines
4. Test with `./build.sh <device>`

### Waveshare Integration Checklist
- [ ] Add I2C relay control functions (replace `digitalWrite()`)
- [ ] Implement RGB LED status feedback (WS2812 on GPIO 38)
- [ ] Add Ethernet support (W5500 via SPI, CS GPIO 10)
- [ ] Implement WiFi fallback with enable/disable config option
- [ ] Prevent WiFi retry loop when Ethernet is active
- [ ] Remove TFT dependencies (use serial logging + RGB LED)
- [ ] Define Waveshare pins in device_config.h
- [ ] Update `versions.json` with "waveshare" device
- [ ] Update `build.sh` with Waveshare-specific flags
- [ ] Test Ethernet → WebSocket → I2C relay trigger flow
- [ ] Test WiFi fallback activation/deactivation
- [ ] Verify RGB LED patterns for all states

## Dependencies
Core libraries (must be in Arduino libraries folder):
- `ArduinoJson` - config parsing + WebSocket payloads
- `WebSocketsClient` - WSS connection to LNbits
- `TFT_eSPI` - display driver (optional, device-dependent, **not used for Waveshare**)
- `Wire.h` - I2C communication (Waveshare relays via TCA9554)
- `Ethernet.h` - W5500 Ethernet support (Waveshare only)
- `FastLED` or `Adafruit_NeoPixel` - WS2812 RGB LED control (Waveshare only)
- `WiFi.h` - WiFi fallback (all devices)

## Common Pitfalls
1. **WebSocket URL Format**: Must start with `wss://` - device validates in setup()
2. **Serial Timing**: Boot mode window is only 2 seconds after power-on
3. **GPIO Safety**: Always `pinMode(pin, OUTPUT)` before `digitalWrite()` to avoid shorts
4. **SPIFFS Size**: Config file storage limited by partition scheme (check `build.sh`)
5. **I2C Expander State**: Always read-modify-write to avoid affecting other relays
6. **WiFi Fallback Loop**: Do NOT continuously retry WiFi if Ethernet is active - causes network slowdowns
7. **Ethernet Cable**: W5500 requires physical cable connected before `Ethernet.begin()` succeeds
8. **RGB LED Timing**: WS2812 is timing-sensitive - disable interrupts during write or use library
9. **Digital Input Initialization**: Configure DI pins as `INPUT_PULLDOWN` after network setup. **Known hardware limitation**: DI 3, 4, 7, 8 indicator LEDs may remain lit or blink despite correct configuration. This does NOT affect DI functionality - pins read correctly and can detect inputs. DI 1, 2, 5, 6 LEDs work normally. Use these for applications requiring visual LED feedback.
10. **Ethernet Configuration**: Use HSPI (SPI2) for W5500 to avoid bus conflicts. Initialize with 20MHz, SPI_MODE0, MSBFIRST settings per manufacturer specs.

## Planned Features (Waveshare)

### 1. Digital Input Monitoring (Lock State Feedback)
**Purpose**: Detect mechanical failure when relay triggers but physical action doesn't complete

**Hardware**:
- Waveshare digital inputs (GPIO 1-8, configurable)
- Connect to NC (normally closed) contact switches
- Use case: Relay triggers solenoid lock → Switch should change from closed (locked) to open (unlocked)

**Behavior**:
- After relay trigger, poll corresponding digital input for state change
- Timeout window: ~500ms-2000ms (configurable)
- If switch doesn't change state → mechanical failure detected
- Report failure via serial log + Telegram alert

**Integration Point**: Add to `executePayment()` after relay trigger:
```cpp
digitalWrite(relay_pin, HIGH);
delay(relay_time);
digitalWrite(relay_pin, LOW);

// Check lock state
bool lockOpened = checkDigitalInput(input_pin, timeout_ms);
if (!lockOpened) {
    Serial.println("WARNING: Lock failed to open!");
    sendTelegramAlert("Lock failure", relay_pin);
}
```

**Config Needs**:
- `input_pin_mapping`: Array mapping relay pins to digital input pins
- `input_check_timeout`: Milliseconds to wait for state change (default 1000ms)
- `input_active_state`: LOW or HIGH when lock is open (default LOW for NC switches)

### 2. Telegram Alerts (Remote Monitoring)
**Purpose**: Notify operator of payment events and lock status across multiple deployed devices

**Message Content**:
- Device identifier (custom name or MAC address)
- Relay number triggered
- Lock state: "Success" or "FAILED"
- Timestamp
- Optional: Payment amount/comment from WebSocket payload

**Example Alert**:
```
🔓 Device: Locker-Room-A
💰 Payment received
🔌 Relay: 3
✅ Lock opened successfully
⏰ 2026-01-17 14:32:05
```

**Integration Points**:
1. `executePayment()` - Send alert after relay + input check
2. `webSocketEvent()` - Optional: Alert on WebSocket disconnect
3. Network setup - Optional: Alert when device comes online

**Config Needs** (add to `elements.json`):
- `telegram_bot_token`: Telegram Bot API token
- `telegram_chat_id`: Chat ID to receive alerts
- `device_name`: Human-readable identifier (e.g., "Locker-Room-A", "Vending-Machine-1")
- `telegram_enabled`: Boolean to enable/disable alerts
- `telegram_commands_enabled`: Enable polling for user commands (optional)

**Dependencies**:
- `UniversalTelegramBot` library or HTTP POST to Telegram API via `HTTPClient`
- WiFiClientSecure for HTTPS to api.telegram.org

**Error Handling**:
- Telegram send failures should NOT block relay operation
- Retry once on failure, then log to serial and continue
- Consider queue for offline alerts (optional, complex)

**Security Note**: Bot token is sensitive - store securely in SPIFFS config file

#### 2.1 Telegram Command Polling (Status Queries)
**Purpose**: Allow remote operator to query device status on demand

**Implementation**:
```cpp
// In loop() - non-blocking check every 5-10 seconds
unsigned long lastTelegramCheck = 0;
if (millis() - lastTelegramCheck > 10000) {
    checkTelegramCommands();
    lastTelegramCheck = millis();
}

void checkTelegramCommands() {
    // Poll Telegram getUpdates API
    // Verify chat_id matches config (authentication)
    if (command == "/status") {
        String report = pollAllDigitalInputs();
        sendTelegramMessage(report);
    }
}
```

**Supported Commands**:
- `/status` - Report all digital input states (locked/unlocked)
- `/check DI3` - Check specific digital input
- `/logs` - Return recent event log from RAM buffer

**Considerations**:
- Must be non-blocking to avoid interfering with WebSocket
- Telegram API rate limit: 30 req/sec (5-10s polling is safe)
- Authenticate via `telegram_chat_id` to prevent unauthorized access
- Only functional when network (Ethernet/WiFi) connected

### 3. Device Logging Strategy
**Purpose**: Persistent event logging for debugging and audit trails (device has no SD card)

**Recommended Hybrid Approach**:

#### 3.1 SPIFFS Persistent Log (Critical Events)
- **Capacity**: ~190KB with `min_spiffs` partition (current config)
- **Retention**: Circular log, auto-rotate when full
- **Events Logged**: Relay triggers, lock failures, network changes, errors
```cpp
File log = SPIFFS.open("/events.log", FILE_APPEND);
log.println("2026-01-17 14:32:05 | Relay 3 | Lock OK");
log.close();
```
**Pros**: Survives reboots, readable via serial config mode  
**Cons**: Limited space (~1000 events), flash wear (~10K writes/sector)

#### 3.2 RAM Circular Buffer (Recent Events)
- **Capacity**: Last 100-200 events in RAM (~4-8KB)
- **Access**: Query via Telegram `/logs` command
```cpp
String logBuffer[100];
int logIndex = 0;
```
**Pros**: Very fast, no flash wear  
**Cons**: Lost on reboot, limited size

#### 3.3 Network Logging (Optional)
**Syslog over UDP** (industry standard):
```cpp
syslog.log(LOG_INFO, "Relay 3 triggered, lock OK");
```
**HTTP POST to cloud endpoint** (e.g., webhook):
```cpp
http.POST("https://yourserver.com/api/logs", jsonPayload);
```
**MQTT broker** (pub/sub model):
```cpp
mqtt.publish("devices/locker-a/events", payload);
```

**Config Needs**:
- `logging_enabled`: Enable SPIFFS logging
- `syslog_server`: IP address for remote syslog (optional, "" to disable)
- `log_retention_hours`: Auto-delete old SPIFFS logs (e.g., 168 = 1 week)

**Log Format** (human + machine readable):
```
2026-01-17 14:32:05 | PAYMENT | Relay:3 | Duration:5000ms | Lock:OK
2026-01-17 14:35:12 | ERROR | Relay:5 | Lock:FAILED | Timeout
2026-01-17 15:00:00 | NETWORK | ETH:OK | WiFi:DISABLED | WSS:Connected
```

---

## Key Files to Understand
- [bitcoinSwitch.ino](bitcoinSwitch/bitcoinSwitch.ino) - WebSocket event handlers
- [100_config.ino](bitcoinSwitch/100_config.ino) - Serial config protocol
- [WAVESHARE_REBUILD_INSTRUCTIONS.md](WAVESHARE_REBUILD_INSTRUCTIONS.md) - Phase-by-phase hardware port guide
- [build.sh](build.sh) - Build flags and device-specific compilation
- [config.js](config.js) - Web installer configuration schema

## Testing
No formal test suite. Verify by:
1. Serial monitor shows "WiFi connected!" and "WebSocket Connected"
2. Send test payment via LNbits → Check relay/GPIO triggers
3. Confirm payload parsing: `{pin-time-comment}` format in `executePayment()`

## Useful Commands
```bash
# Monitor serial output only
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Clean build artifacts
rm -rf build/

# Check git history for original code
git log --oneline bitcoinSwitch/
```
