/*
  POTA_Library_WiFi.ino - Example for POTA library
  ------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    This example demonstrates using POTA with built-in Wi-Fi handling.
    The sketch connects to Wi-Fi, initializes POTA with credentials
    from secrets.h, and performs a one-shot OTA update check.

  Usage:
    - Edit secrets.h to set:
        * WIFI_SSID, WIFI_PASSWORD → your Wi-Fi network
        * DEVICE_TYPE → select the correct board (ESP32 / ESP8266 / Opta)
        * FIRMWARE_VERSION → firmware version string
        * AUTH_TOKEN, SERVER_SECRET → values obtained from registering
          your device at pleasedontcode.com (Please Over The Air service)

  Compatible boards:
    - ESP32
    - ESP8266
    - Arduino Opta WiFi
*/

#include "secrets.h" // Contains WIFI_SSID, WIFI_PASSWORD, DEVICE_TYPE, FIRMWARE_VERSION, AUTH_TOKEN, SERVER_SECRET
#include <POTA.h>

POTA ota;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // ℹ️ Print firmware + device info
  Serial.println("\n🔧 Starting device...");
  Serial.print("💻 Device Type: ");
  Serial.println(DEVICE_TYPE);
  Serial.print("📦 Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);

  // 1️⃣ Library handles Wi-Fi connection internally
  POTAError err = ota.begin(WIFI_SSID, WIFI_PASSWORD, DEVICE_TYPE, FIRMWARE_VERSION, AUTH_TOKEN, SERVER_SECRET);
  if (err != POTAError::SUCCESS) {
    Serial.print("\n❌ POTA begin failed: ");
    Serial.println(ota.errorToString(err));
    return;
  }
  Serial.println("✅ Wi-Fi and POTA initialized");

  // 2️⃣ Check and perform OTA once
  err = ota.checkAndPerformOTA();
  if (err == POTAError::NO_UPDATE_AVAILABLE) {
    Serial.println("✅ Firmware already up to date");
  } else if (err != POTAError::SUCCESS) {
    Serial.print("❌ OTA error: ");
    Serial.println(ota.errorToString(err));
  }
}

void loop() {
  // main code
}
