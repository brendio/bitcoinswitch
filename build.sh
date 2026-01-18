#!/bin/sh
if [ -z "$1" ]; then
    echo "Usage: sh build.sh <device>"
    echo "Available devices: esp32, tdisplay, waveshare"
    exit 1
fi

device_name=$(echo $1 | tr '[:lower:]' '[:upper:]')

# Device-specific configuration
fqbn="esp32:esp32:ttgo-lora32"
extra_flags=""

if [ "$1" = "waveshare" ]; then
    echo "Building for Waveshare ESP32-S3-ETH-8DI-8RO..."
    fqbn="esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc"
    device_name="WAVESHARE"
    
    # Define Waveshare-specific flags
    waveshare_defines="-DWAVESHARE -DUSE_RGB_LED -DUSE_ETHERNET -DUSE_I2C_RELAY -DETH_CLK_MODE=ETH_CLOCK_GPIO0_IN"
    
    # Build with explicit compiler flags
    arduino-cli compile \
        --libraries libraries \
        --build-property "build.partitions=min_spiffs" \
        --build-property "upload.maximum_size=1966080" \
        --build-property "compiler.cpp.extra_flags=${waveshare_defines}" \
        --build-property "compiler.c.extra_flags=${waveshare_defines}" \
        --build-path build \
        --fqbn $fqbn bitcoinSwitch
    exit 0
else
    # Check for TFT configuration (other devices)
    tft_config_file="tft_config_$1.txt"
    if [ -f "$tft_config_file" ]; then
        echo "Using TFT configuration file $tft_config_file."
        user_tft_config=$(cat $tft_config_file | tr '\n' ' ' | sed -e "s/\ /\ -D/g" -e "s/-D$//")
        tft_font="-DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1 -DLOAD_FONT6=1 -DLOAD_FONT7=1 -DLOAD_FONT8=1 -DLOAD_GFXFF=1 -DSMOOTH_FONT=1"
        tft_config=" -DTFT=1 -DUSER_SETUP_LOADED=1 -D${user_tft_config} ${tft_font} -DSPI_FREQUENCY=27000000 -DSPI_READ_FREQUENCY=20000000"
        extra_flags="${tft_config}"
    fi
fi

echo "Device: $device_name"
echo "FQBN: $fqbn"
echo "Extra flags: $extra_flags"

arduino-cli compile \
    --libraries libraries \
    --build-property "build.partitions=min_spiffs" \
    --build-property "upload.maximum_size=1966080" \
    --build-property "build.extra_flags.esp32=${extra_flags}" \
    --build-path build \
    --fqbn $fqbn bitcoinSwitch
