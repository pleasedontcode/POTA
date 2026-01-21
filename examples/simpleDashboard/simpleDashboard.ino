/*
  simpleDashboard.ino - POTA Dashboard Basic Example
  ---------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    Simple dashboard example showing basic sensor monitoring.
    Perfect introduction to POTA dashboard widgets.
    
    Features:
    - Temperature, humidity, and air quality monitoring
    - Automatic sensor value updates
    - Read-only display widgets
    - No user interaction required

  Scenario:
    Home weather station with environmental sensors

  Widgets Used:
    - TEMPERATURE_CARD: Shows temperature in °C
    - HUMIDITY_CARD: Shows humidity percentage
    - AIR_CARD: Shows air quality index (0-100)
    - GENERIC_CARD: Shows system status

  Configuration:
    Edit secrets.h to set Wi-Fi and POTA credentials

  Compatible Boards:
    - ESP32
    - ESP8266
    - Arduino Opta WiFi
*/

#include <POTA.h>
#include "secrets.h"

POTA pota;

// Widget IDs
uint8_t tempWidget;
uint8_t humidityWidget;
uint8_t airQualityWidget;
uint8_t statusWidget;

// Sensor state
float temperature = 22.5;
float humidity = 55.0;
int airQuality = 85;

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 3000; // Update every 3 seconds

/**
 * @brief Configure all dashboard widgets
 * 
 * Called automatically by POTA when dashboard connects.
 * Define all your widgets here.
 */
void setupDashboard() {
  Serial.println("🎨 Setting up dashboard widgets...");
  
  // Create sensor display widgets
  tempWidget = pota.dashboard.addWidget(TEMPERATURE_CARD, "Living Room");
  humidityWidget = pota.dashboard.addWidget(HUMIDITY_CARD, "Living Room");
  airQualityWidget = pota.dashboard.addWidget(AIR_CARD, "Air Quality");
  statusWidget = pota.dashboard.addWidget(GENERIC_CARD, "System Status");
  
  // Set initial values
  pota.dashboard.setValue(tempWidget, temperature);
  pota.dashboard.setValue(humidityWidget, humidity);
  pota.dashboard.setValue(airQualityWidget, airQuality);
  pota.dashboard.setValue(statusWidget, "Online");
  
  Serial.println("✅ Dashboard configured!");
}

/**
 * @brief Simulate sensor readings
 * 
 * In a real application, replace this with actual sensor code.
 * Example: temperature = dht.readTemperature();
 */
void readSensors() {
  // Simulate temperature fluctuation (±0.5°C)
  temperature += random(-5, 6) / 10.0;
  temperature = constrain(temperature, 18.0, 28.0);
  
  // Simulate humidity fluctuation (±2%)
  humidity += random(-20, 21) / 10.0;
  humidity = constrain(humidity, 40.0, 70.0);
  
  // Simulate air quality changes
  airQuality += random(-3, 4);
  airQuality = constrain(airQuality, 60, 100);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   POTA Simple Dashboard Example        ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize POTA with Wi-Fi
  Serial.println("🔧 Initializing POTA...");
  POTAError err = pota.begin(
    DEVICE_TYPE, 
    FIRMWARE_VERSION, 
    AUTH_TOKEN, 
    SERVER_SECRET,
    WIFI_SSID, 
    WIFI_PASSWORD
  );
  
  if (err != POTAError::SUCCESS) {
    Serial.print("❌ POTA initialization failed: ");
    Serial.println(POTA::errorToString(err));
    return;
  }
  
  Serial.println("✅ POTA initialized");
  
  // Register dashboard setup callback
  pota.dashboard.setWidgetConfigCallback(setupDashboard);
  
  Serial.println("✅ Setup complete\n");
}

void loop() {
  pota.loop();
  
  // Update sensor readings periodically
  if (millis() - lastUpdate > UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Read sensors (simulated)
    readSensors();
    
    // Update dashboard values
    pota.dashboard.setValue(tempWidget, temperature);
    pota.dashboard.setValue(humidityWidget, humidity);
    pota.dashboard.setValue(airQualityWidget, airQuality);
    
    // Print to serial for debugging
    Serial.print("📊 Temp: ");
    Serial.print(temperature, 1);
    Serial.print("°C | Humidity: ");
    Serial.print(humidity, 1);
    Serial.print("% | Air Quality: ");
    Serial.println(airQuality);
  }
}
