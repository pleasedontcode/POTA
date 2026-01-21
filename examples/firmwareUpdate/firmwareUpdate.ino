/*
  firmwareUpdate.ino - Firmware Update Over The Air Example
  ---------------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    Demonstrates safe OTA updates using POTA library with WebSocket notifications.
    
    Features:
    - Automatic Wi-Fi connection managed by POTA
    - Boot-time firmware update checks
    - WebSocket push notifications for instant updates
    - Safety validation before device reboot

  Update Strategy:
    1. Check for updates at every boot (setup)
    2. WebSocket push notification → validate safety → reboot → update at next boot
    
    This ensures updates are resilient to failures and always attempted at boot.

  Configuration:
    Edit secrets.h to set:
    - WIFI_SSID, WIFI_PASSWORD: Your Wi-Fi network credentials
    - DEVICE_TYPE: Platform (ESP32 / ESP8266 / Opta)
    - FIRMWARE_VERSION: Current firmware version
    - AUTH_TOKEN, SERVER_SECRET: Credentials from pleasedontcode.com

  Compatible Boards:
    - ESP32
    - ESP8266
    - Arduino Opta WiFi
*/

#include "secrets.h"
#include <POTA.h>

POTA pota;

bool updateAvailableNeedsReboot = false;  // Flag set by WebSocket, triggers reboot when safe
bool criticalOperationRunning = false;    // Set to true during critical tasks

/**
 * @brief Callback invoked when new firmware is available via WebSocket
 * 
 * Sets flag to trigger safety checks in loop(). When conditions are safe,
 * device reboots and update is applied at next boot.
 * 
 * @param version Available firmware version
 */
void onFirmwareAvailable(const char* version) {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   NEW FIRMWARE UPDATE AVAILABLE!       ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.print("📦 New Version: ");
  Serial.println(version);
  Serial.print("📦 Current Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("⏳ Waiting for safe conditions to reboot...\n");
  
  updateAvailableNeedsReboot = true;
}

/**
 * @brief Validates device is safe for reboot/update
 * 
 * Add your custom safety checks here:
 * - Battery level checks
 * - Business hours validation
 * - Sensor reading validation
 * - Active user sessions
 * - Data synchronization status
 * 
 * @return true if safe to reboot, false otherwise
 */
bool isSafeToUpdate() {
  if (criticalOperationRunning) {
    return false;
  }
  
  // Example: Battery level check (uncomment for battery-powered devices)
  // int batteryLevel = analogRead(BATTERY_PIN);
  // if (batteryLevel < 20) return false;
  
  // Example: Business hours check (avoid updates during work hours)
  // struct tm timeinfo;
  // if (getLocalTime(&timeinfo)) {
  //   if (timeinfo.tm_hour >= 9 && timeinfo.tm_hour < 17) return false;
  // }
  
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println(F("\n╔════════════════════════════════════════╗"));
  Serial.println(F("║     POTA - Please Over The Air         ║"));
  Serial.println(F("║      Safe OTA Update Example           ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));
  
  Serial.print(F("💻 Device Type: "));
  Serial.println(DEVICE_TYPE);
  Serial.print(F("📦 Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("🔑 Device MAC: "));
  Serial.println(pota.getSecureMACAddress());

  // Initialize POTA with built-in Wi-Fi management
  Serial.println(F("\n🔧 Initializing POTA with built-in Wi-Fi..."));
  
  POTAError err = pota.begin(
    DEVICE_TYPE, 
    FIRMWARE_VERSION, 
    AUTH_TOKEN, 
    SERVER_SECRET,
    WIFI_SSID, 
    WIFI_PASSWORD
  );
  
  
  // Alternative: External Wi-Fi management
  // If you manage Wi-Fi externally, use instead something like that:
  /*
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connected");
  
  POTAError err = pota.begin(
    DEVICE_TYPE,
    FIRMWARE_VERSION,
    AUTH_TOKEN,
    SERVER_SECRET
    // No SSID/password - using existing connection
  );
  */
  
  if (err != POTAError::SUCCESS) {
    Serial.print(F("❌ POTA initialization failed: "));
    Serial.println(POTA::errorToString(err));
    Serial.println(F("Please check your credentials in secrets.h"));
    return;
  }
  Serial.println(F("✅ POTA initialized successfully"));
  Serial.println(F("✅ Wi-Fi connected and managed by POTA"));

  // Register callback for WebSocket update notifications
  pota.onOTAAvailable(onFirmwareAvailable);
  
  // Check for updates at boot
  Serial.println(F("🔍 Checking for firmware updates..."));
  err = pota.checkAndPerformOTA();
  
  if (err == POTAError::NO_UPDATE_AVAILABLE) {
    Serial.println(F("✅ Firmware is up to date\n"));
  } else if (err != POTAError::SUCCESS) {
    Serial.print(F("⚠️ OTA check failed: "));
    Serial.println(POTA::errorToString(err));
    Serial.println(F("Will retry at next boot\n"));
  }
  
  Serial.println(F("✅ Setup complete - entering main loop"));
  Serial.println(F("📊 WebSocket notifications enabled\n"));
}

void loop() {
  // Handle POTA communication (WebSocket, auto-reconnect)
  pota.loop();
  
  // Check if update notification received and conditions are safe
  if (updateAvailableNeedsReboot && isSafeToUpdate()) {
    Serial.println("\n[OTA] ✅ Safe conditions met - rebooting...\n");
    delay(1000);
    pota.restart();
  }
  
  // ════════════════════════════════════════════════════════════
  // YOUR APPLICATION CODE
  // ════════════════════════════════════════════════════════════
  
  // Example: Critical operation simulation
  static unsigned long criticalOpStart = 0;
  static bool inCriticalOp = false;
  
  if (millis() - criticalOpStart > 60000 && !inCriticalOp) {
    Serial.println("[App] Starting critical operation...");
    criticalOperationRunning = true;
    inCriticalOp = true;
    criticalOpStart = millis();
  }
  
  if (inCriticalOp && millis() - criticalOpStart > 10000) {
    Serial.println("[App] Critical operation completed");
    criticalOperationRunning = false;
    inCriticalOp = false;
  }
}
