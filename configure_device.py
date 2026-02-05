#!/usr/bin/env python3
"""
bitcoinSwitch Configuration Tool
Writes configuration to device via serial port

Usage:
    python3 configure_device.py [port]
    
    port: Serial port (default: /dev/cu.usbmodem101)
    
Before running:
    1. Edit config_template.json with your values
    2. Ensure device is in config mode (blue fast blink LED)
    3. Install pyserial: pip3 install pyserial
"""

import serial
import json
import sys
import time
from pathlib import Path

DEFAULT_PORT = "/dev/cu.usbmodem101"
CONFIG_FILE = "config_local.json"
BAUDRATE = 115200
TIMEOUT = 2

def load_config_template():
    """Load configuration from JSON template file"""
    config_path = Path(__file__).parent / CONFIG_FILE
    
    if not config_path.exists():
        print(f"Error: Config template not found: {config_path}")
        print("Creating default template...")
        create_default_template(config_path)
        print(f"✅ Created {CONFIG_FILE} with default values")
        print(f"📝 Edit {CONFIG_FILE} with your settings and run again")
        sys.exit(1)
    
    with open(config_path, 'r') as f:
        return json.load(f)

def create_default_template(path):
    """Create default config template file"""
    default_config = {
        "_comment": "bitcoinSwitch Configuration Template - Edit values below",
        "config_ssid": "YourWiFiSSID",
        "config_password": "YourWiFiPassword",
        "config_device_string": "wss://your.lnbits.com/bitcoinswitch/api/v1/ws/YOUR_DEVICE_ID",
        
        "_comment_optional": "Optional parameters - leave empty to disable features",
        "telegram_bot_token": "",
        "telegram_chat_id": "",
        "device_name": "Waveshare-01",
        
        "_comment_ethernet": "Ethernet static IP config - leave blank for DHCP (recommended)",
        "static_ip": "",
        "static_gateway": "",
        "static_subnet": "255.255.255.0",
        
        "_comment_legacy": "Legacy threshold mode parameters (disabled in v1.0+, kept for compatibility)",
        "config_threshold_inkey": "",
        "config_threshold_amount": "",
        "config_threshold_pin": "",
        "config_threshold_time": "",
        
        "_comment_di_monitoring": "Digital Input monitoring (optional, uses 1:1 mapping)",
        "di_monitor_enabled": "false",
        "di_check_timeout_ms": "2000",
        
        "_comment_logging": "Event logging configuration (optional)",
        "logging_enabled": "true",
        "log_retention_hours": "168",
        "syslog_server": "",
        "syslog_port": "514"
    }
    
    with open(path, 'w') as f:
        json.dump(default_config, f, indent=2)

def validate_config(config):
    """Validate configuration values"""
    errors = []
    warnings = []
    
    # Required fields
    device_string = config.get("config_device_string", "")
    if not device_string:
        errors.append("config_device_string is required")
    elif not device_string.startswith("wss://"):
        errors.append("config_device_string must start with wss:// (secure WebSocket)")
    elif device_string == "wss://your.lnbits.com/bitcoinswitch/api/v1/ws/YOUR_DEVICE_ID":
        errors.append("config_device_string still has placeholder value - update with your actual LNbits URL")
    
    # WiFi validation (optional but warn if looks incomplete)
    ssid = config.get("config_ssid", "")
    password = config.get("config_password", "")
    if ssid == "YourWiFiSSID":
        warnings.append("WiFi SSID has placeholder value - update or leave blank to use Ethernet only")
    if ssid and not password:
        warnings.append("WiFi SSID set but password is empty - WiFi may not connect")
    if password and not ssid:
        warnings.append("WiFi password set but SSID is empty")
    
    # Telegram validation
    bot_token = config.get("telegram_bot_token", "")
    chat_id = config.get("telegram_chat_id", "")
    if bot_token and not chat_id:
        errors.append("telegram_chat_id required when telegram_bot_token is set")
    if chat_id and not bot_token:
        errors.append("telegram_bot_token required when telegram_chat_id is set")
    
    # Static IP validation (basic format check)
    static_ip = config.get("static_ip", "")
    if static_ip:
        parts = static_ip.split('.')
        if len(parts) != 4:
            errors.append(f"static_ip '{static_ip}' is not a valid IPv4 address")
    
    return errors, warnings

def check_config_mode(ser, timeout=3.0):
    """Check if device is in config mode and ready to accept commands"""
    print("🔍 Checking if device is in config mode...")
    
    # Flush buffers
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # Send file-list command (safe, doesn't modify anything)
    ser.write(b'/file-list\n')
    time.sleep(0.5)
    
    # Read response
    start_time = time.time()
    response_lines = []
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                response_lines.append(line)
                # Don't print every line during detection
        else:
            time.sleep(0.1)
    
    # Check for config mode indicators
    if not response_lines:
        return False, "No response from device"
    
    response_text = ' '.join(response_lines).lower()
    
    # Look for config mode markers
    if 'config mode' in response_text or 'available commands' in response_text or 'file-list' in response_text:
        return True, "Device is in config mode"
    
    # Check if device is running normally (not in config mode)
    if any(indicator in response_text for indicator in ['wifi', 'ethernet', 'websocket', 'connected', 'network']):
        return False, "Device is running normally (not in config mode)"
    
    # Unknown state
    return False, "Could not determine device state"

def prompt_config_mode_entry():
    """Guide user to enter config mode"""
    print("\n⚠️  Device is not in config mode!")
    print("\n📖 To enter config mode:")
    print("   • Device auto-enters if no valid config exists")
    print("   • OR power cycle and look for BLUE FAST BLINK LED")
    print("   • OR send reset command (option 't' below)")
    print("\nOptions:")
    print("   [r] Retry detection (if config mode is now active)")
    print("   [t] Trigger reset to enter config mode")
    print("   [w] Wait 10 seconds and retry")
    print("   [q] Quit")
    
    choice = input("\nYour choice [r/t/w/q]: ").lower()
    return choice

def reset_device_software(ser):
    """Send software reset command to device"""
    print("🔄 Sending software reset command...")
    ser.write(b'/reset\n')
    time.sleep(0.3)
    
    # Read any response
    response = []
    while ser.in_waiting > 0:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            response.append(line)
            print(f"  {line}")
    
    if response and 'reboot' in ' '.join(response).lower():
        print("✅ Software reset command acknowledged")
        time.sleep(2.5)  # Wait for device to reboot
        return True
    else:
        print("⚠️  No response to software reset")
        return False

def reset_device_hardware(ser):
    """Reset device using DTR/RTS pins (hardware method)"""
    print("🔄 Attempting hardware reset via DTR/RTS...")
    try:
        ser.setDTR(False)
        ser.setRTS(True)
        time.sleep(0.1)
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setDTR(True)
        time.sleep(2.5)  # Wait for boot
        print("✅ Hardware reset complete")
        return True
    except Exception as e:
        print(f"⚠️  Hardware reset failed: {e}")
        return False

def reset_device(ser):
    """Reset device using software command, fallback to hardware"""
    # Try software reset first
    if reset_device_software(ser):
        return True
    
    # Fallback to hardware reset
    print("Trying hardware reset as fallback...")
    return reset_device_hardware(ser)

def write_config_to_device(port, config):
    """Write configuration to device via serial"""
    print(f"📡 Connecting to {port}...")
    
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=TIMEOUT)
        time.sleep(1.0)  # Let serial settle
        
        # Flush any pending data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # CHECK CONFIG MODE
        is_config_mode, status_msg = check_config_mode(ser)
        
        if not is_config_mode:
            print(f"❌ {status_msg}")
            
            # Interactive retry loop
            while True:
                choice = prompt_config_mode_entry()
                
                if choice == 'r':
                    # Retry detection
                    is_config_mode, status_msg = check_config_mode(ser)
                    if is_config_mode:
                        print(f"✅ {status_msg}")
                        break
                    else:
                        print(f"❌ {status_msg}")
                        continue
                
                elif choice == 't':
                    # Trigger reset
                    if reset_device(ser):
                        print("⏳ Waiting for device to enter config mode...")
                        time.sleep(1.0)
                        is_config_mode, status_msg = check_config_mode(ser)
                        if is_config_mode:
                            print(f"✅ {status_msg}")
                            break
                        else:
                            print(f"❌ {status_msg}")
                            print("💡 Device may need to be manually power cycled")
                            continue
                    else:
                        print("❌ Reset failed")
                        continue
                
                elif choice == 'w':
                    # Wait and retry
                    print("\n⏳ Waiting 10 seconds...")
                    time.sleep(10)
                    is_config_mode, status_msg = check_config_mode(ser)
                    if is_config_mode:
                        print(f"✅ {status_msg}")
                        break
                    else:
                        print(f"❌ {status_msg}")
                        continue
                
                elif choice == 'q':
                    print("❌ Cancelled by user")
                    ser.close()
                    return False
                
                else:
                    print("Invalid choice")
                    continue
        else:
            print(f"✅ {status_msg}")
        
        print("🗑️  Removing old config...")
        ser.write(b'/file-remove\n')
        time.sleep(0.5)
        
        # Clear any response
        while ser.in_waiting > 0:
            ser.readline()
        
        # Filter out comment fields and build config array
        config_array = []
        for key, value in config.items():
            if not key.startswith("_"):  # Skip comment fields
                config_array.append({"name": key, "value": str(value)})
        
        print(f"✍️  Writing configuration ({len(config_array)} parameters)...")
        
        # Write opening bracket
        ser.write(b'/file-append [\n')
        time.sleep(0.1)
        
        # Write each config item as compact JSON on one line
        for i, item in enumerate(config_array):
            # Serialize each object without whitespace
            item_json = json.dumps(item, separators=(',', ':'))
            
            # Add comma for all but last item
            if i < len(config_array) - 1:
                item_json += ","
            
            cmd = f'/file-append {item_json}\n'
            ser.write(cmd.encode('utf-8'))
            time.sleep(0.1)
            
            # Progress indicator
            if (i + 1) % 3 == 0 or i == len(config_array) - 1:
                print(f"   Written {i + 1}/{len(config_array)} parameters...")
        
        # Write closing bracket
        ser.write(b'/file-append ]\n')
        time.sleep(0.2)
        
        # Flush and wait
        ser.flush()
        time.sleep(0.5)
        
        print("🔍 Verifying configuration...")
        ser.write(b'/file-read /elements.json\n')
        time.sleep(1.0)
        
        # Read and display response
        print("\n=== Device Response ===")
        json_lines = []
        response_count = 0
        while ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"  {line}")
                response_count += 1
                # Extract JSON content (remove /file-send prefix if present)
                if '/file-send' in line:
                    content = line.split('/file-send', 1)[-1].strip()
                    if content:
                        json_lines.append(content)
        
        if response_count == 0:
            print("  ⚠️  No response - config may not have been written")
        else:
            # Try to validate JSON
            if json_lines:
                reconstructed = ''.join(json_lines)
                try:
                    parsed = json.loads(reconstructed)
                    print(f"\n✅ Validation: Config has {len(parsed)} valid parameters")
                except json.JSONDecodeError as e:
                    print(f"\n⚠️  Warning: JSON validation failed - {e}")
        
        print("\n✅ Finalizing configuration...")
        ser.write(b'/config-done\n')
        ser.flush()
        time.sleep(0.5)
        
        # Read final response
        while ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"  {line}")
        
        print("\n✅ Configuration complete! Device will exit config mode.")
        
        print(f"📊 Monitor device: arduino-cli monitor -p {port} -c baudrate=115200")
        
        ser.close()
        return True
        
    except serial.SerialException as e:
        print(f"\n❌ Serial error: {e}")
        print(f"   Make sure device is connected to {port}")
        print(f"   On macOS, list ports with: ls /dev/cu.usbmodem*")
        print(f"   On Linux, try: ls /dev/ttyUSB* /dev/ttyACM*")
        return False
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return False
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    print("╔════════════════════════════════════════╗")
    print("║  bitcoinSwitch Configuration Tool     ║")
    print("╚════════════════════════════════════════╝\n")
    
    # Get port from command line or use default
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    
    # Load config template
    config = load_config_template()
    
    # Validate
    errors, warnings = validate_config(config)
    
    # Show warnings
    if warnings:
        print("⚠️  Warnings:")
        for warning in warnings:
            print(f"   • {warning}")
        print()
    
    # Show errors and exit if any
    if errors:
        print("❌ Configuration errors:")
        for error in errors:
            print(f"   • {error}")
        print(f"\n📝 Edit {CONFIG_FILE} to fix these issues")
        sys.exit(1)
    
    # Display config summary
    print("📋 Configuration summary:")
    ssid = config.get('config_ssid', '')
    if ssid and ssid != "YourWiFiSSID":
        print(f"   WiFi: {ssid}")
    else:
        print(f"   WiFi: (disabled - Ethernet only)")
    
    device_string = config.get('config_device_string', '')
    if len(device_string) > 60:
        print(f"   LNbits: {device_string[:57]}...")
    else:
        print(f"   LNbits: {device_string}")
    
    telegram_enabled = bool(config.get('telegram_bot_token'))
    print(f"   Telegram: {'✅ Enabled' if telegram_enabled else '❌ Disabled'}")
    print(f"   Device Name: {config.get('device_name', 'Not set')}")
    
    static_ip = config.get('static_ip', '')
    if static_ip:
        print(f"   Static IP: {static_ip}")
    else:
        print(f"   Network: DHCP (auto)")
    
    print(f"   Serial Port: {port}")
    print()
    
    # Confirm
    try:
        response = input("✋ Write this config to device? [y/N]: ")
        if response.lower() != 'y':
            print("❌ Cancelled.")
            sys.exit(0)
    except KeyboardInterrupt:
        print("\n❌ Cancelled.")
        sys.exit(0)
    
    print()
    
    # Write to device
    success = write_config_to_device(port, config)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
