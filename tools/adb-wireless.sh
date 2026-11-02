#!/bin/bash

REQUEST_SERIAL="$1"

main()
{
    # Check if at least 1 device is connected
    DEVICE_COUNT=$(adb devices | grep -w device | wc -l)
    if [[ "$DEVICE_COUNT" -eq '0' ]]; then
        echo "No device connected. Please connect at least 1 device to run the script."
        exit 1
    fi

    if [[ "$DEVICE_COUNT" -eq '1' ]]; then
        ANDROID_SERIAL=$(adb devices | grep -w device | awk '{print $1}')
    fi

    if [[ -z "$ANDROID_SERIAL" ]]; then
        ANDROID_SERIAL="${REQUEST_SERIAL:-$ANDROID_SERIAL}"
    fi

    if [[ -z "$ANDROID_SERIAL" ]]; then
        echo "No device serial provided. Please provide a device serial or set the ANDROID_SERIAL environment variable."
        exit 1
    fi

    adb shell cmd wifi set-wifi-enabled enabled
    adb shell cmd wifi connect-network "Datalogic-RD01" wpa2 "Mc2Sol25!!"
    for i in {1..15}; do
        if [[ -n "$(adb shell dumpsys connectivity | grep -E "NetworkAgentInfo.*WIFI" | grep -oE "CONNECTED|DISCONNECTED")" ]]; then
            break
        fi
        sleep 1
    done

    adb disconnect

    IP_ADDRESS="$(adb shell ip addr show wlan0 | grep 'inet ' | awk '{print $2}' | cut -d/ -f1)"
    PORT='5555'
    
    if [[ -z "$IP_ADDRESS" ]]; then
        echo "Failed to retrieve the device's IP address. Please check the wifi connection and try again."
        exit 1
    fi
    sleep 1
    
    adb tcpip "$PORT"
    sleep 1

    adb connect "$IP_ADDRESS:$PORT"
    echo "Device is connected to wifi."
}

main 