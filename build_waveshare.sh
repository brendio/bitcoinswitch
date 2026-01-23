#!/bin/bash
# Test script for Waveshare ESP32-S3-ETH-8DI-8RO Hardware
# Tests I2C relay controller and RGB LED

echo "=== Waveshare Hardware Test ==="
echo ""
echo "This test will:"
echo "1. Compile the firmware for Waveshare"
echo "2. Upload to /dev/cu.usbmodem101"
echo "3. Monitor serial output"
echo ""
echo "Expected behavior:"
echo "- Blue LED at boot"
echo "- I2C relay controller initializes"
echo "- RGB LED color test"
echo "- WiFi connection (if configured)"
echo ""

# Check if device is connected
if [ ! -e "/dev/cu.usbmodem101" ]; then
    echo "ERROR: Device not found at /dev/cu.usbmodem101"
    echo "Please connect the Waveshare device via USB"
    exit 1
fi

echo "Device found at /dev/cu.usbmodem101"
echo ""

# Build firmware
echo "Building firmware..."
./build.sh waveshare

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo ""
echo "Build successful!"
echo ""

# Upload firmware
echo "Uploading firmware to device..."
arduino-cli upload \
    --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
    --port /dev/cu.usbmodem101 \
    --input-dir build

if [ $? -ne 0 ]; then
    echo "ERROR: Upload failed"
    exit 1
fi

echo ""
echo "Upload successful!"
echo ""
echo "Monitoring serial output (Ctrl+C to exit)..."
echo "=========================================="
sleep 2

# Monitor serial output
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
