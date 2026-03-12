#!/bin/bash

# Configuration
PACKAGE_NAME="com.example.psi"
MAIN_ACTIVITY="com.example.psi.MainActivity"
APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
# Use 'adb' by default, or ./adb if the user specifically has it in the current dir
ADB="/home/pratyut/Android/Sdk/platform-tools/adb"

# Check if ./adb exists, if not use global adb
if [ ! -f "$ADB" ]; then
    ADB="adb"
fi

echo "--- 1. Compiling Android Application ---"
./gradlew assembleDebug
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "--- 2. Uninstalling existing application ---"
$ADB uninstall $PACKAGE_NAME

echo "--- 3. Installing new application ---"
$ADB install $APK_PATH
if [ $? -ne 0 ]; then
    echo "Installation failed!"
    exit 1
fi

echo "--- 4. Launching application ---"
$ADB shell am start -n $PACKAGE_NAME/$MAIN_ACTIVITY

echo "--- Done! ---"
