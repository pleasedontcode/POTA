/*
  chartsDashboard.ino - POTA Dashboard Charts Example
  ----------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/programming-over-the-air/

  Description:
    Advanced dashboard example showing data visualization with charts.
    Learn how to use different chart types and update them dynamically.
    
    Features:
    - Line chart for temperature trends with scrolling history
    - Bar chart for daily energy consumption
    - Pie chart for energy usage distribution by appliance
    - Dynamic data updates with real-time correlation
    - Interactive controls to influence chart data
    
    Data Correlation:
    - Temperature affects cooling energy consumption
    - High temperature → increased cooling usage in pie chart
    - Energy consumption accumulated in bar chart
    - All data points are interconnected and reactive

  Scenario:
    Smart home energy monitoring and environmental tracking system

  Widgets Used:
    - LINE_CHART: Temperature trend over time (scrolling)
    - BAR_CHART: Daily energy consumption (kWh)
    - PIE_CHART: Energy usage distribution by appliance
    - SLIDER_CARD: Temperature setpoint control
    - TOGGLE_BUTTON_CARD: Cooling system on/off
    - Display cards for current values
    
  Step Parameter in Sliders:
    The 'step' parameter controls increment precision:
    - step = 1 → Integer values (18, 19, 20, 21...)
    - step = 0.5 → Half-degree precision (18.0, 18.5, 19.0...)
    - step = 0.1 → Tenth-degree precision (18.0, 18.1, 18.2...)

  Chart Data Management:
    - setX(): Set X-axis labels (strings or numbers)
    - setY(): Set Y-axis values (numbers)
    - Arrays must remain in scope (use global or static)
    - Call setX() and setY() before sendUpdates()

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
uint8_t headerSeparator;
uint8_t tempCard;
uint8_t energyCard;
uint8_t coolingCard;
uint8_t tempSetpointSlider;
uint8_t coolingToggle;
uint8_t lineChartWidget;
uint8_t barChartWidget;
uint8_t pieChartWidget;

// Current sensor values
float currentTemp = 22.5;
float tempSetpoint = 21.0;      // Desired temperature
bool coolingActive = false;     // Cooling system state
float currentEnergy = 1234.0;
float todayEnergy = 0.0;        // Today's energy accumulator

// Line chart data - Temperature trend (last 10 readings)
float temperatureHistory[10] = {20.0, 21.0, 21.5, 22.0, 22.5, 23.0, 22.8, 22.5, 22.3, 22.5};
const char* timeLabels[10] = {"T-9", "T-8", "T-7", "T-6", "T-5", "T-4", "T-3", "T-2", "T-1", "Now"};

// Bar chart data - Daily energy consumption (kWh)
int dailyEnergy[7] = {15, 18, 12, 20, 22, 17, 0};  // Last value = today (accumulating)
const char* dayLabels[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// Pie chart data - Energy usage by appliance (percentage)
// Values update dynamically based on temperature and cooling state
const char* applianceLabels[4] = {"Heating", "Cooling", "Lighting", "Other"};
int applianceUsage[4] = {30, 20, 15, 35};

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 5000; // Update every 5 seconds

/**
 * @brief Configure all dashboard widgets
 */
void setupDashboard() {
  Serial.println("🎨 Setting up charts dashboard...");
  
  // Create header separator with instructions
  headerSeparator = pota.dashboard.addWidget(SEPARATOR_CARD, "📊 Smart Energy Monitor - Interactive Demo", 0, "Set your target temperature and toggle cooling to see real-time correlation between temperature, energy consumption, and usage distribution across all charts");
  
  // Create display cards
  tempCard = pota.dashboard.addWidget(TEMPERATURE_CARD, "Current Temp");
  energyCard = pota.dashboard.addWidget(ENERGY_CARD, "Total Energy");
  coolingCard = pota.dashboard.addWidget(FEEDBACK_CARD, "Cooling System");
  
  // Create control widgets
  // Temperature setpoint with 0.5°C steps (values: 18.0, 18.5, 19.0, etc.)
  tempSetpointSlider = pota.dashboard.addWidget(SLIDER_CARD, "Target Temp", 0, "", 0.5, 18, 28);
  coolingToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Cooling Control");
  
  // Create chart widgets
  lineChartWidget = pota.dashboard.addWidget(LINE_CHART, "Temperature Trend");
  barChartWidget = pota.dashboard.addWidget(BAR_CHART, "Weekly Energy (kWh)");
  pieChartWidget = pota.dashboard.addWidget(PIE_CHART, "Energy Distribution");
  
  // Set initial values
  pota.dashboard.setValue(tempCard, currentTemp);
  pota.dashboard.setValue(energyCard, currentEnergy);
  pota.dashboard.setValue(coolingCard, STATUS_NONE);
  pota.dashboard.setValue(tempSetpointSlider, tempSetpoint);
  pota.dashboard.setValue(coolingToggle, coolingActive);
  
  // Initialize charts
  updateCharts();
  
  // Setup callbacks
  setupCallbacks();
  
  Serial.println("✅ Dashboard configured!");
}

/**
 * @brief Register widget interaction callbacks
 */
void setupCallbacks() {
  // Temperature setpoint callback
  pota.dashboard.onUpdate(tempSetpointSlider, [](WidgetData data) {
    tempSetpoint = data.getFloat();
    Serial.print("🌡️ Target temperature set to: ");
    Serial.print(tempSetpoint, 1);
    Serial.println("°C");
  });
  
  // Cooling toggle callback
  pota.dashboard.onUpdate(coolingToggle, [](WidgetData data) {
    coolingActive = data.getBool();
    Serial.print("❄️ Cooling system: ");
    Serial.println(coolingActive ? "ON" : "OFF");
    
    // Update feedback status
    pota.dashboard.setValue(coolingCard, coolingActive ? STATUS_INFO : STATUS_NONE);
  });
}

/**
 * @brief Update all chart data
 */
void updateCharts() {
  // Update line chart - Temperature trend
  pota.dashboard.setX(lineChartWidget, timeLabels, 10);
  pota.dashboard.setY(lineChartWidget, temperatureHistory, 10);
  
  // Update bar chart - Weekly energy consumption
  pota.dashboard.setX(barChartWidget, dayLabels, 7);
  pota.dashboard.setY(barChartWidget, dailyEnergy, 7);
  
  // Update pie chart - Usage distribution
  pota.dashboard.setX(pieChartWidget, applianceLabels, 4);
  pota.dashboard.setY(pieChartWidget, applianceUsage, 4);
}

/**
 * @brief Simulate sensor reading and update history with correlated behavior
 * 
 * Temperature behavior:
 * - Rises naturally over time (ambient heat)
 * - Cooling system actively reduces temperature when ON
 * - Rate of change depends on difference from setpoint
 */
void updateSensorData() {
  // Temperature dynamics with correlation
  float tempDelta = 0.0;
  
  if (coolingActive) {
    // Cooling is active - temperature moves toward setpoint
    if (currentTemp > tempSetpoint) {
      tempDelta = -0.3;  // Cool down
    } else {
      tempDelta = random(-2, 3) / 10.0;  // Maintain with small fluctuations
    }
  } else {
    // Cooling is off - temperature naturally rises
    tempDelta = random(0, 8) / 10.0;  // Gradual warming
  }
  
  currentTemp += tempDelta;
  currentTemp = constrain(currentTemp, 18.0, 32.0);
  
  // Energy consumption - correlated to cooling activity and temperature difference
  float energyIncrement = 0.5;  // Base consumption (lighting, other)
  
  if (coolingActive) {
    // Cooling energy proportional to temperature difference
    float tempDiff = abs(currentTemp - tempSetpoint);
    energyIncrement += tempDiff * 0.5;  // More difference = more energy
  }
  
  currentEnergy += energyIncrement;
  todayEnergy += energyIncrement;
  
  // Update pie chart based on current conditions
  if (coolingActive && currentTemp > tempSetpoint) {
    // High cooling demand
    applianceUsage[0] = 10;  // Heating (minimal)
    applianceUsage[1] = 50;  // Cooling (high)
    applianceUsage[2] = 15;  // Lighting
    applianceUsage[3] = 25;  // Other
  } else if (currentTemp < tempSetpoint) {
    // Would need heating
    applianceUsage[0] = 45;  // Heating (high)
    applianceUsage[1] = 5;   // Cooling (minimal)
    applianceUsage[2] = 15;  // Lighting
    applianceUsage[3] = 35;  // Other
  } else {
    // Balanced
    applianceUsage[0] = 25;  // Heating
    applianceUsage[1] = 25;  // Cooling
    applianceUsage[2] = 15;  // Lighting
    applianceUsage[3] = 35;  // Other
  }
  
  // Shift temperature history (scrolling effect)
  for (int i = 0; i < 9; i++) {
    temperatureHistory[i] = temperatureHistory[i + 1];
  }
  temperatureHistory[9] = currentTemp; // Add new reading
  
  // Update today's energy in bar chart
  dailyEnergy[6] = (int)todayEnergy;
  
  Serial.print("📊 Temp: ");
  Serial.print(currentTemp, 1);
  Serial.print("°C (target: ");
  Serial.print(tempSetpoint, 1);
  Serial.print("°C) | Energy: ");
  Serial.print(currentEnergy, 1);
  Serial.print(" kWh | Cooling: ");
  Serial.println(coolingActive ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   POTA Charts Dashboard Example        ║");
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
  
  Serial.println("✅ Setup complete");
  Serial.println("📈 Charts will update every 5 seconds\n");
}

void loop() {
  pota.loop();
  
  // Update sensor data and charts periodically
  if (millis() - lastUpdate > UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Read sensors and update history with correlation
    updateSensorData();
    
    // Update dashboard cards
    pota.dashboard.setValue(tempCard, currentTemp);
    pota.dashboard.setValue(energyCard, currentEnergy);
    
    // Update cooling status based on activity and effectiveness
    if (coolingActive) {
      if (currentTemp <= tempSetpoint) {
        pota.dashboard.setValue(coolingCard, STATUS_SUCCESS);  // Target reached
      } else {
        pota.dashboard.setValue(coolingCard, STATUS_INFO);     // Actively cooling
      }
    } else {
      pota.dashboard.setValue(coolingCard, STATUS_NONE);       // Off
    }
    
    // Update charts with new correlated data
    updateCharts();
    
    // Send updates to connected clients
    pota.dashboard.sendUpdates();
  }
}
