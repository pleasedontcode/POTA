/*
  fullDashboard.ino - POTA Complete Dashboard Example
  ----------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    Complete dashboard demonstration with all available widgets in a
    coherent, real-world scenario: Smart Greenhouse Automation System.
    
    This example showcases a professional IoT application for remote
    greenhouse management with environmental monitoring, automated
    climate control, irrigation management, and comprehensive analytics.

  Scenario: Smart Greenhouse Automation System
    A commercial greenhouse operator needs to remotely monitor and control:
    - Environmental conditions (temperature, humidity, light, CO2)
    - Automated climate control (heating, cooling, ventilation)
    - Irrigation and fertilization systems
    - Growth analytics and historical trends
    - Equipment status and alerts
    - Remote configuration and scheduling

  Tabs Organization:
    1. Overview: Real-time environmental dashboard
    2. Climate Control: Temperature, humidity, ventilation management
    3. Irrigation: Water and fertilizer control
    4. Analytics: Growth trends and performance metrics

  All Widgets Demonstrated:
    Display: TEMPERATURE_CARD, HUMIDITY_CARD, AIR_CARD, ENERGY_CARD, 
             GENERIC_CARD, PROGRESS_CARD, FEEDBACK_CARD, INDICATOR_BUTTON_CARD
    Controls: TOGGLE_BUTTON_CARD, SLIDER_CARD, JOYSTICK_2D_CARD, 
              JOYSTICK_X_CARD, JOYSTICK_Y_CARD, RANGE_SLIDER_CARD,
              DROPDOWN_CARD, TEXT_INPUT_CARD, PASSWORD_CARD, 
              COLOR_PICKER_CARD, PUSH_BUTTON_CARD, ACTION_BUTTON_CARD
    Media: LINK_CARD, IMAGE_CARD, FILE_UPLOAD_CARD
    Charts: BAR_CHART, LINE_CHART, AREA_CHART, PIE_CHART, DOUGHNUT_CHART
    Layout: TAB, SEPARATOR_CARD

  Configuration:
    Edit secrets.h to set Wi-Fi and POTA credentials

  Compatible Boards:
    - ESP32
    - ESP8266
    - Arduino Opta WiFi
*/

#include <POTA.h>
#include "secrets.h"
#include <time.h>

POTA pota;

// ════════════════════════════════════════════════════════════
// WIDGET IDs STRUCTURE
// ════════════════════════════════════════════════════════════
struct WidgetIDs {
  // Tabs
  uint8_t tabOverview;
  uint8_t tabClimate;
  uint8_t tabIrrigation;
  uint8_t tabAnalytics;
  
  // Tab Headers
  uint8_t separatorOverview;
  uint8_t separatorClimate;
  uint8_t separatorIrrigation;
  uint8_t separatorAnalytics;
  
  // TAB 1: OVERVIEW - Environmental Monitoring
  uint8_t tempCard;
  uint8_t humidityCard;
  uint8_t co2Card;
  uint8_t lightCard;
  uint8_t soilMoistureProgress;
  uint8_t systemStatus;
  uint8_t waterLevelIndicator;
  uint8_t manualLink;
  uint8_t cameraImage;
  uint8_t refreshCameraBtn;
  
  // TAB 2: CLIMATE CONTROL
  uint8_t heatingToggle;
  uint8_t coolingToggle;
  uint8_t ventilationToggle;
  uint8_t tempRangeSlider;
  uint8_t humidityRangeSlider;
  uint8_t fanSpeedSlider;
  uint8_t climateMode;
  uint8_t scheduleUpload;
  uint8_t climateStatus;
  
  // TAB 3: IRRIGATION
  uint8_t mainPumpToggle;
  uint8_t fertilizerToggle;
  uint8_t zone1Toggle;
  uint8_t zone2Toggle;
  uint8_t waterFlowSlider;
  uint8_t fertilizerConcentration;
  uint8_t irrigationJoystick2D;
  uint8_t valve1Joystick;
  uint8_t valve2Joystick;
  uint8_t pHSlider;
  uint8_t ecSlider;
  uint8_t cropTypeDropdown;
  uint8_t operatorName;
  uint8_t operatorPassword;
  uint8_t nutrientColor;
  uint8_t irrigationSchedule;
  uint8_t weekDaySelector;
  uint8_t timeSync;
  uint8_t timeDifference;
  uint8_t calibratePumpBtn;
  uint8_t emergencyStopBtn;
  uint8_t irrigationStatus;
  
  // TAB 4: ANALYTICS
  uint8_t tempTrendLine;
  uint8_t humidityTrendArea;
  uint8_t weeklyWaterBar;
  uint8_t energyDistributionPie;
  uint8_t cropHealthDoughnut;
} widgets;

// ════════════════════════════════════════════════════════════
// GREENHOUSE STATE
// ════════════════════════════════════════════════════════════
struct GreenhouseState {
  // Environmental Sensors
  float temperature = 24.5;
  float humidity = 68.0;
  int co2Level = 420;        // ppm
  float lightIntensity = 650.0; // lux
  int soilMoisture = 45;     // percentage
  
  // Climate Control
  bool heatingActive = false;
  bool coolingActive = false;
  bool ventilationActive = true;
  float tempMin = 18.0;
  float tempMax = 28.0;
  float humidityMin = 60.0;
  float humidityMax = 80.0;
  int fanSpeed = 50;
  int climateMode = 0;       // 0=Auto, 1=Manual, 2=Eco, 3=Night
  
  // Irrigation System
  bool mainPumpOn = false;
  bool fertilizerOn = false;
  bool zone1Active = false;
  bool zone2Active = false;
  float waterFlow = 5.0;     // L/min
  float fertilizerConc = 2.5; // g/L
  float pHLevel = 6.5;
  float ecLevel = 1.8;       // mS/cm
  int cropType = 0;          // 0=Tomatoes, 1=Lettuce, 2=Peppers, 3=Herbs
  String operatorName = "Admin";
  
  // Valve control (joystick)
  float valve1Position = 0.0;
  float valve2Position = 0.0;
  
  // System Status
  int waterLevel = 75;       // percentage in tank
  float energyConsumption = 234.5; // kWh
  
  // Chart Data - Temperature History (24 hours)
  float tempHistory[24];
  float humidityHistory[24];
  
  // Weekly Water Usage (7 days)
  int weeklyWater[7] = {120, 135, 128, 145, 132, 140, 0}; // liters, last = today
  
  // Energy Distribution
  const char* energyLabels[4] = {"Heating", "Cooling/Vent", "Pumps", "Lighting"};
  int energyValues[4] = {35, 25, 20, 20}; // percentage
  
  // Crop Health Distribution
  const char* healthLabels[4] = {"Excellent", "Good", "Fair", "Poor"};
  int healthValues[4] = {45, 35, 15, 5}; // percentage of plants
  
  int currentHour = 0;       // Simulated hour for data scrolling
  float todayWaterUsage = 0.0;
} state;

unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 3000; // 3 seconds

// Chart X-axis labels
const char* hourLabels[24] = {
  "00:00", "01:00", "02:00", "03:00", "04:00", "05:00",
  "06:00", "07:00", "08:00", "09:00", "10:00", "11:00",
  "12:00", "13:00", "14:00", "15:00", "16:00", "17:00",
  "18:00", "19:00", "20:00", "21:00", "22:00", "23:00"
};

const char* dayLabels[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// ════════════════════════════════════════════════════════════
// DASHBOARD SETUP
// ════════════════════════════════════════════════════════════
void setupDashboard() {
  Serial.println("🎨 Setting up Smart Greenhouse Dashboard...");
  
  // Initialize temperature history
  for (int i = 0; i < 24; i++) {
    state.tempHistory[i] = 22.0 + random(-20, 30) / 10.0;
    state.humidityHistory[i] = 65.0 + random(-50, 50) / 10.0;
  }
  
  // ──────────────────────────────────────────────────────────
  // TABS
  // ──────────────────────────────────────────────────────────
  widgets.tabOverview = pota.dashboard.addWidget(TAB, "🧭 Overview");
  widgets.tabClimate = pota.dashboard.addWidget(TAB, "🌤️ Climate");
  widgets.tabIrrigation = pota.dashboard.addWidget(TAB, "🚿 Irrigation");
  widgets.tabAnalytics = pota.dashboard.addWidget(TAB, "📉 Analytics");
  
  // ──────────────────────────────────────────────────────────
  // TAB 1: OVERVIEW - Environmental Dashboard
  // ──────────────────────────────────────────────────────────
  widgets.separatorOverview = pota.dashboard.addWidget(SEPARATOR_CARD, 
    "🌱 Greenhouse Environmental Monitor", 
    widgets.tabOverview, 
    "Real-time monitoring of critical greenhouse parameters: temperature, humidity, CO2, light intensity, and soil moisture. Camera feed updates every refresh");
  
  widgets.tempCard = pota.dashboard.addWidget(TEMPERATURE_CARD, "Temperature", widgets.tabOverview);
  widgets.humidityCard = pota.dashboard.addWidget(HUMIDITY_CARD, "Humidity", widgets.tabOverview);
  widgets.co2Card = pota.dashboard.addWidget(AIR_CARD, "CO2 Level", widgets.tabOverview);
  widgets.lightCard = pota.dashboard.addWidget(ENERGY_CARD, "Light Intensity", widgets.tabOverview);
  
  widgets.soilMoistureProgress = pota.dashboard.addWidget(PROGRESS_CARD, "Soil Moisture", widgets.tabOverview, "", 1, 0, 100);
  widgets.systemStatus = pota.dashboard.addWidget(GENERIC_CARD, "System Mode", widgets.tabOverview);
  widgets.waterLevelIndicator = pota.dashboard.addWidget(INDICATOR_BUTTON_CARD, "Water Tank Level", widgets.tabOverview);
  widgets.climateStatus = pota.dashboard.addWidget(FEEDBACK_CARD, "Climate Status", widgets.tabOverview);
  
  widgets.manualLink = pota.dashboard.addWidget(LINK_CARD, "User Manual", widgets.tabOverview, "https://github.com/pleasedontcode/POTA");
  widgets.cameraImage = pota.dashboard.addWidget(IMAGE_CARD, "Greenhouse Camera", widgets.tabOverview, "https://picsum.photos/400/300");
  widgets.refreshCameraBtn = pota.dashboard.addWidget(PUSH_BUTTON_CARD, "Refresh Camera", widgets.tabOverview);
  
  // ──────────────────────────────────────────────────────────
  // TAB 2: CLIMATE CONTROL
  // ──────────────────────────────────────────────────────────
  widgets.separatorClimate = pota.dashboard.addWidget(SEPARATOR_CARD, 
    "🌡️ Climate Control System", 
    widgets.tabClimate, 
    "Automated climate management: toggle heating/cooling/ventilation, set temperature and humidity ranges, adjust fan speed, select operation mode. Upload schedule for automated control");
  
  widgets.heatingToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Heating System", widgets.tabClimate);
  widgets.coolingToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Cooling System", widgets.tabClimate);
  widgets.ventilationToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Ventilation", widgets.tabClimate);
  
  // Temperature range with 0.5°C precision
  widgets.tempRangeSlider = pota.dashboard.addWidget(RANGE_SLIDER_CARD, "Temp Range (°C)", widgets.tabClimate, "", 0.5, 10, 40);
  widgets.humidityRangeSlider = pota.dashboard.addWidget(RANGE_SLIDER_CARD, "Humidity Range (%)", widgets.tabClimate, "", 1, 40, 95);
  
  // Fan speed with integer steps
  widgets.fanSpeedSlider = pota.dashboard.addWidget(SLIDER_CARD, "Fan Speed", widgets.tabClimate, "", 5, 0, 100);
  
  widgets.climateMode = pota.dashboard.addWidget(DROPDOWN_CARD, "Climate Mode", widgets.tabClimate, "Auto,Manual,Eco,Night");
  widgets.scheduleUpload = pota.dashboard.addWidget(FILE_UPLOAD_CARD, "Upload Schedule", widgets.tabClimate);
  
  // ──────────────────────────────────────────────────────────
  // TAB 3: IRRIGATION SYSTEM
  // ──────────────────────────────────────────────────────────
  widgets.separatorIrrigation = pota.dashboard.addWidget(SEPARATOR_CARD, 
    "💧 Irrigation & Fertigation Control", 
    widgets.tabIrrigation, 
    "Complete water and fertilizer management: control pumps and zones, adjust flow rates and concentrations, monitor pH/EC levels, select crop type for optimal nutrition. Use joysticks for precise valve control. Emergency stop available");
  
  widgets.mainPumpToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Main Pump", widgets.tabIrrigation);
  widgets.fertilizerToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Fertilizer Pump", widgets.tabIrrigation);
  widgets.zone1Toggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Zone 1 (North)", widgets.tabIrrigation);
  widgets.zone2Toggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "Zone 2 (South)", widgets.tabIrrigation);
  
  // Water flow with 0.1 L/min precision
  widgets.waterFlowSlider = pota.dashboard.addWidget(SLIDER_CARD, "Water Flow (L/min)", widgets.tabIrrigation, "", 0.1, 0, 20);
  // Fertilizer with 0.1 g/L precision
  widgets.fertilizerConcentration = pota.dashboard.addWidget(SLIDER_CARD, "Fertilizer (g/L)", widgets.tabIrrigation, "", 0.1, 0, 10);
  
  // Manual valve control with joysticks
  widgets.irrigationJoystick2D = pota.dashboard.addWidget(JOYSTICK_2D_CARD, "Main Valve Control", widgets.tabIrrigation);
  widgets.valve1Joystick = pota.dashboard.addWidget(JOYSTICK_X_CARD, "Zone 1 Valve", widgets.tabIrrigation);
  widgets.valve2Joystick = pota.dashboard.addWidget(JOYSTICK_Y_CARD, "Zone 2 Valve", widgets.tabIrrigation);
  
  // pH and EC monitoring with 0.1 precision
  widgets.pHSlider = pota.dashboard.addWidget(SLIDER_CARD, "pH Level", widgets.tabIrrigation, "", 0.1, 4, 8);
  widgets.ecSlider = pota.dashboard.addWidget(SLIDER_CARD, "EC Level (mS/cm)", widgets.tabIrrigation, "", 0.1, 0, 5);
  
  widgets.cropTypeDropdown = pota.dashboard.addWidget(DROPDOWN_CARD, "Crop Type", widgets.tabIrrigation, "Tomatoes,Lettuce,Peppers,Herbs");
  
  widgets.operatorName = pota.dashboard.addWidget(TEXT_INPUT_CARD, "Operator Name", widgets.tabIrrigation);
  widgets.operatorPassword = pota.dashboard.addWidget(PASSWORD_CARD, "Auth Code", widgets.tabIrrigation);
  
  widgets.nutrientColor = pota.dashboard.addWidget(COLOR_PICKER_CARD, "Nutrient Indicator", widgets.tabIrrigation);
  
  // Scheduling widgets
  widgets.irrigationSchedule = pota.dashboard.addWidget(SEPARATOR_CARD, "⏰ Irrigation Schedule", widgets.tabIrrigation, "");
  widgets.weekDaySelector = pota.dashboard.addWidget(WEEK_SELECTION_CARD, "Active Days", widgets.tabIrrigation);
  widgets.timeSync = pota.dashboard.addWidget(TIME_SYNC_CARD, "Start Time", widgets.tabIrrigation);
  widgets.timeDifference = pota.dashboard.addWidget(TIME_DIFFERENCE_CARD, "Duration", widgets.tabIrrigation);
  
  widgets.calibratePumpBtn = pota.dashboard.addWidget(PUSH_BUTTON_CARD, "Calibrate Pump", widgets.tabIrrigation);
  widgets.emergencyStopBtn = pota.dashboard.addWidget(ACTION_BUTTON_CARD, "EMERGENCY STOP", widgets.tabIrrigation, "Do you confirm?");
  
  widgets.irrigationStatus = pota.dashboard.addWidget(FEEDBACK_CARD, "Irrigation Status", widgets.tabIrrigation);
  
  // ──────────────────────────────────────────────────────────
  // TAB 4: ANALYTICS & TRENDS
  // ──────────────────────────────────────────────────────────
  widgets.separatorAnalytics = pota.dashboard.addWidget(SEPARATOR_CARD, 
    "📊 Growth Analytics & Performance", 
    widgets.tabAnalytics, 
    "Comprehensive data visualization: 24-hour temperature and humidity trends, weekly water consumption, energy usage distribution, and crop health status. All charts update automatically with live data");
  
  widgets.tempTrendLine = pota.dashboard.addWidget(LINE_CHART, "Temperature Trend (24h)", widgets.tabAnalytics);
  widgets.humidityTrendArea = pota.dashboard.addWidget(AREA_CHART, "Humidity Trend (24h)", widgets.tabAnalytics);
  widgets.weeklyWaterBar = pota.dashboard.addWidget(BAR_CHART, "Weekly Water Usage (L)", widgets.tabAnalytics);
  widgets.energyDistributionPie = pota.dashboard.addWidget(PIE_CHART, "Energy Distribution", widgets.tabAnalytics);
  widgets.cropHealthDoughnut = pota.dashboard.addWidget(DOUGHNUT_CHART, "Crop Health Status", widgets.tabAnalytics);
  
  Serial.println("✅ All widgets created!");
  
  // Set initial values
  setInitialValues();
  
  // Setup callbacks
  setupCallbacks();
}

// ════════════════════════════════════════════════════════════
// SET INITIAL VALUES
// ════════════════════════════════════════════════════════════
void setInitialValues() {
  // Overview tab
  pota.dashboard.setValue(widgets.tempCard, state.temperature);
  pota.dashboard.setValue(widgets.humidityCard, state.humidity);
  pota.dashboard.setValue(widgets.co2Card, state.co2Level);
  pota.dashboard.setValue(widgets.lightCard, state.lightIntensity);
  pota.dashboard.setValue(widgets.soilMoistureProgress, state.soilMoisture);
  pota.dashboard.setValue(widgets.systemStatus, "Auto Mode");
  pota.dashboard.setValue(widgets.waterLevelIndicator, STATUS_SUCCESS);
  pota.dashboard.setValue(widgets.climateStatus, STATUS_SUCCESS);
  
  // Climate tab
  pota.dashboard.setValue(widgets.heatingToggle, state.heatingActive);
  pota.dashboard.setValue(widgets.coolingToggle, state.coolingActive);
  pota.dashboard.setValue(widgets.ventilationToggle, state.ventilationActive);
  pota.dashboard.setValue(widgets.fanSpeedSlider, state.fanSpeed);
  pota.dashboard.setValue(widgets.climateMode, "Auto");
  
  // Irrigation tab
  pota.dashboard.setValue(widgets.mainPumpToggle, state.mainPumpOn);
  pota.dashboard.setValue(widgets.fertilizerToggle, state.fertilizerOn);
  pota.dashboard.setValue(widgets.zone1Toggle, state.zone1Active);
  pota.dashboard.setValue(widgets.zone2Toggle, state.zone2Active);
  pota.dashboard.setValue(widgets.waterFlowSlider, state.waterFlow);
  pota.dashboard.setValue(widgets.fertilizerConcentration, state.fertilizerConc);
  pota.dashboard.setValue(widgets.pHSlider, state.pHLevel);
  pota.dashboard.setValue(widgets.ecSlider, state.ecLevel);
  pota.dashboard.setValue(widgets.cropTypeDropdown, "Tomatoes");
  pota.dashboard.setValue(widgets.operatorName, state.operatorName);
  pota.dashboard.setValue(widgets.nutrientColor, "#4CAF50");
  pota.dashboard.setValue(widgets.irrigationStatus, STATUS_NONE);
  
  // Initialize TIME_DIFFERENCE_CARD with current time in ISO format
  // This sets the baseline for "time since last sync"
  time_t now = time(nullptr);
  struct tm* ti = gmtime(&now);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", ti);
  pota.dashboard.setValue(widgets.timeDifference, String(buffer));
  
  // Initialize charts
  updateCharts();
}

// ════════════════════════════════════════════════════════════
// CALLBACKS SETUP
// ════════════════════════════════════════════════════════════
void setupCallbacks() {
  Serial.println("🔗 Setting up callbacks...");
  
  // ──── OVERVIEW TAB ────
  pota.dashboard.onUpdate(widgets.refreshCameraBtn, [](WidgetData data) {
    Serial.println("📸 Refreshing camera feed...");
    pota.dashboard.setValue(widgets.cameraImage, "https://picsum.photos/400/300?" + String(random(1, 10000)));
  });
  
  pota.dashboard.onUpdate(widgets.waterLevelIndicator, [](WidgetData data) {
    Serial.println("💧 Refilling water tank...");
    
    // Simulate tank refill process
    state.waterLevel = 100;
    
    // Update indicator to green (full)
    pota.dashboard.setValue(widgets.waterLevelIndicator, STATUS_SUCCESS);
    
    // Update feedback
    pota.dashboard.setValue(widgets.climateStatus, STATUS_SUCCESS);
    
    Serial.println("✅ Water tank refilled to 100%");
  });
  
  // ──── CLIMATE TAB ────
  pota.dashboard.onUpdate(widgets.heatingToggle, [](WidgetData data) {
    state.heatingActive = data.getBool();
    Serial.print("🔥 Heating: ");
    Serial.println(state.heatingActive ? "ON" : "OFF");
    updateClimateStatus();
  });
  
  pota.dashboard.onUpdate(widgets.coolingToggle, [](WidgetData data) {
    state.coolingActive = data.getBool();
    Serial.print("❄️ Cooling: ");
    Serial.println(state.coolingActive ? "ON" : "OFF");
    updateClimateStatus();
  });
  
  pota.dashboard.onUpdate(widgets.ventilationToggle, [](WidgetData data) {
    state.ventilationActive = data.getBool();
    Serial.print("💨 Ventilation: ");
    Serial.println(state.ventilationActive ? "ON" : "OFF");
  });
  
  pota.dashboard.onUpdate(widgets.tempRangeSlider, [](WidgetData data) {
    state.tempMin = data.min;
    state.tempMax = data.max;
    Serial.print("🌡️ Temp Range: ");
    Serial.print(state.tempMin, 1);
    Serial.print(" - ");
    Serial.print(state.tempMax, 1);
    Serial.println("°C");
  });
  
  pota.dashboard.onUpdate(widgets.humidityRangeSlider, [](WidgetData data) {
    state.humidityMin = data.min;
    state.humidityMax = data.max;
    Serial.print("💧 Humidity Range: ");
    Serial.print(state.humidityMin, 0);
    Serial.print(" - ");
    Serial.print(state.humidityMax, 0);
    Serial.println("%");
  });
  
  pota.dashboard.onUpdate(widgets.fanSpeedSlider, [](WidgetData data) {
    state.fanSpeed = data.getInt();
    Serial.print("💨 Fan Speed: ");
    Serial.print(state.fanSpeed);
    Serial.println("%");
  });
  
  pota.dashboard.onUpdate(widgets.climateMode, [](WidgetData data) {
    String mode = data.getString();
    Serial.print("⚙️ Climate Mode: ");
    Serial.println(mode);
    pota.dashboard.setValue(widgets.systemStatus, mode + " Mode");
  });
  
  pota.dashboard.onUpdate(widgets.scheduleUpload, [](WidgetData data) {
    Serial.print("📁 Schedule uploaded: ");
    Serial.println(data.getString());
  });
  
  // ──── IRRIGATION TAB ────
  pota.dashboard.onUpdate(widgets.mainPumpToggle, [](WidgetData data) {
    state.mainPumpOn = data.getBool();
    Serial.print("⚡ Main Pump: ");
    Serial.println(state.mainPumpOn ? "ON" : "OFF");
    updateIrrigationStatus();
  });
  
  pota.dashboard.onUpdate(widgets.fertilizerToggle, [](WidgetData data) {
    state.fertilizerOn = data.getBool();
    Serial.print("🧪 Fertilizer: ");
    Serial.println(state.fertilizerOn ? "ON" : "OFF");
    updateIrrigationStatus();
  });
  
  pota.dashboard.onUpdate(widgets.zone1Toggle, [](WidgetData data) {
    state.zone1Active = data.getBool();
    Serial.print("🌿 Zone 1: ");
    Serial.println(state.zone1Active ? "ACTIVE" : "INACTIVE");
  });
  
  pota.dashboard.onUpdate(widgets.zone2Toggle, [](WidgetData data) {
    state.zone2Active = data.getBool();
    Serial.print("🌿 Zone 2: ");
    Serial.println(state.zone2Active ? "ACTIVE" : "INACTIVE");
  });
  
  pota.dashboard.onUpdate(widgets.waterFlowSlider, [](WidgetData data) {
    state.waterFlow = data.getFloat();
    Serial.print("💧 Water Flow: ");
    Serial.print(state.waterFlow, 1);
    Serial.println(" L/min");
  });
  
  pota.dashboard.onUpdate(widgets.fertilizerConcentration, [](WidgetData data) {
    state.fertilizerConc = data.getFloat();
    Serial.print("🧪 Fertilizer: ");
    Serial.print(state.fertilizerConc, 1);
    Serial.println(" g/L");
  });
  
  pota.dashboard.onUpdate(widgets.irrigationJoystick2D, [](WidgetData data) {
    Serial.print("🕹️ Main Valve: X=");
    Serial.print(data.x);
    Serial.print(" Y=");
    Serial.print(data.y);
    Serial.print(" Dir=");
    Serial.println(data.direction);
  });
  
  pota.dashboard.onUpdate(widgets.valve1Joystick, [](WidgetData data) {
    state.valve1Position = data.x;
    Serial.print("🔧 Valve 1: ");
    Serial.println(state.valve1Position);
  });
  
  pota.dashboard.onUpdate(widgets.valve2Joystick, [](WidgetData data) {
    state.valve2Position = data.y;
    Serial.print("🔧 Valve 2: ");
    Serial.println(state.valve2Position);
  });
  
  pota.dashboard.onUpdate(widgets.pHSlider, [](WidgetData data) {
    state.pHLevel = data.getFloat();
    Serial.print("🧪 pH: ");
    Serial.println(state.pHLevel, 1);
  });
  
  pota.dashboard.onUpdate(widgets.ecSlider, [](WidgetData data) {
    state.ecLevel = data.getFloat();
    Serial.print("⚡ EC: ");
    Serial.print(state.ecLevel, 1);
    Serial.println(" mS/cm");
  });
  
  pota.dashboard.onUpdate(widgets.cropTypeDropdown, [](WidgetData data) {
    Serial.print("🌱 Crop Type: ");
    Serial.println(data.getString());
  });
  
  pota.dashboard.onUpdate(widgets.operatorName, [](WidgetData data) {
    state.operatorName = data.getString();
    Serial.print("👤 Operator: ");
    Serial.println(state.operatorName);
  });
  
  pota.dashboard.onUpdate(widgets.operatorPassword, [](WidgetData data) {
    Serial.println("🔒 Authentication code updated");
  });
  
  pota.dashboard.onUpdate(widgets.nutrientColor, [](WidgetData data) {
    Serial.print("🎨 Nutrient Indicator: ");
    Serial.println(data.getString());
  });
  
  pota.dashboard.onUpdate(widgets.calibratePumpBtn, [](WidgetData data) {
    Serial.println("🔧 Pump calibration started...");
    pota.dashboard.setValue(widgets.irrigationStatus, STATUS_INFO);
  });
  
  pota.dashboard.onUpdate(widgets.emergencyStopBtn, [](WidgetData data) {
    Serial.println("🚨 EMERGENCY STOP ACTIVATED!");
    
    // Stop all pumps and zones
    state.mainPumpOn = false;
    state.fertilizerOn = false;
    state.zone1Active = false;
    state.zone2Active = false;
    
    // Update dashboard
    pota.dashboard.setValue(widgets.mainPumpToggle, false);
    pota.dashboard.setValue(widgets.fertilizerToggle, false);
    pota.dashboard.setValue(widgets.zone1Toggle, false);
    pota.dashboard.setValue(widgets.zone2Toggle, false);
    pota.dashboard.setValue(widgets.irrigationStatus, STATUS_DANGER);
  });
  
  // ──── SCHEDULING WIDGETS ────
  pota.dashboard.onUpdate(widgets.weekDaySelector, [](WidgetData data) {
    Serial.print("📅 Active Days Selected: ");
    Serial.println(data.getString());
  });
  
  // TIME_SYNC_CARD callback - receives browser timestamp and updates TIME_DIFFERENCE_CARD
  pota.dashboard.onUpdate(widgets.timeSync, [](WidgetData data) {
    // Get timestamp as string to avoid losing precision
    String timeStr = data.getString();
    
    // Convert milliseconds to seconds (remove last 3 digits)
    long long ms = atoll(timeStr.c_str());
    time_t rawtime = (time_t)(ms / 1000);
    
    // Format to ISO 8601 string (YYYY-MM-DDTHH:MM:SSZ)
    struct tm* ti = gmtime(&rawtime); // Use UTC time
    
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", ti);
    String isoString = String(buffer);
    
    Serial.print("⏰ Time Sync - Received (ms): ");
    Serial.println(timeStr);
    Serial.print("⏰ Converted to ISO: ");
    Serial.println(isoString);
    
    // Update TIME_DIFFERENCE_CARD with ISO formatted timestamp
    // The widget will automatically calculate and display elapsed time
    pota.dashboard.setValue(widgets.timeDifference, isoString);
  });
  
  pota.dashboard.onUpdate(widgets.timeDifference, [](WidgetData data) {
    Serial.print("⏱️ Duration Set: ");
    Serial.println(data.getString());
  });
  
  Serial.println("✅ Callbacks registered!");
}

// ════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ════════════════════════════════════════════════════════════
void updateClimateStatus() {
  if (state.heatingActive || state.coolingActive) {
    pota.dashboard.setValue(widgets.climateStatus, STATUS_INFO);
  } else {
    pota.dashboard.setValue(widgets.climateStatus, STATUS_SUCCESS);
  }
}

void updateIrrigationStatus() {
  if (state.mainPumpOn && state.fertilizerOn) {
    pota.dashboard.setValue(widgets.irrigationStatus, STATUS_SUCCESS);
  } else if (state.mainPumpOn) {
    pota.dashboard.setValue(widgets.irrigationStatus, STATUS_INFO);
  } else {
    pota.dashboard.setValue(widgets.irrigationStatus, STATUS_NONE);
  }
}

// ════════════════════════════════════════════════════════════
// SENSOR SIMULATION
// ════════════════════════════════════════════════════════════
void updateSensors() {
  // Temperature control logic
  if (state.heatingActive && state.temperature < state.tempMax) {
    state.temperature += 0.3;
  } else if (state.coolingActive && state.temperature > state.tempMin) {
    state.temperature -= 0.2;
  } else {
    state.temperature += random(-3, 4) / 10.0;
  }
  state.temperature = constrain(state.temperature, 15.0, 35.0);
  
  // Humidity affected by ventilation
  if (state.ventilationActive) {
    state.humidity += random(-5, 3) / 10.0;
  } else {
    state.humidity += random(-2, 6) / 10.0;
  }
  state.humidity = constrain(state.humidity, 50.0, 90.0);
  
  // CO2 level
  state.co2Level += random(-10, 15);
  state.co2Level = constrain(state.co2Level, 350, 800);
  
  // Light intensity (simulated day/night cycle)
  state.lightIntensity += random(-50, 50);
  state.lightIntensity = constrain(state.lightIntensity, 100, 1000);
  
  // Soil moisture affected by irrigation
  if (state.mainPumpOn && (state.zone1Active || state.zone2Active)) {
    state.soilMoisture += 2;
  } else {
    state.soilMoisture -= 1;
  }
  state.soilMoisture = constrain(state.soilMoisture, 20, 80);
  
  // Water consumption
  if (state.mainPumpOn) {
    float waterUsed = state.waterFlow * (UPDATE_INTERVAL / 1000.0) / 60.0; // Convert to liters
    state.todayWaterUsage += waterUsed;
    state.waterLevel -= 0.1;
  }
  state.waterLevel = constrain(state.waterLevel, 0, 100);
  
  // Update water level indicator status
  if (state.waterLevel < 20) {
    pota.dashboard.setValue(widgets.waterLevelIndicator, STATUS_DANGER);
  } else if (state.waterLevel < 50) {
    pota.dashboard.setValue(widgets.waterLevelIndicator, STATUS_WARNING);
  } else {
    pota.dashboard.setValue(widgets.waterLevelIndicator, STATUS_SUCCESS);
  }
  
  // Energy distribution based on active systems
  state.energyValues[0] = state.heatingActive ? 45 : 15;  // Heating
  state.energyValues[1] = state.coolingActive || state.ventilationActive ? 35 : 15;  // Cooling/Vent
  state.energyValues[2] = state.mainPumpOn ? 30 : 10;     // Pumps
  state.energyValues[3] = 20;                              // Lighting (constant)
}

// ════════════════════════════════════════════════════════════
// CHARTS UPDATE
// ════════════════════════════════════════════════════════════
void updateCharts() {
  // Scroll temperature and humidity history
  for (int i = 0; i < 23; i++) {
    state.tempHistory[i] = state.tempHistory[i + 1];
    state.humidityHistory[i] = state.humidityHistory[i + 1];
  }
  state.tempHistory[23] = state.temperature;
  state.humidityHistory[23] = state.humidity;
  
  // Update today's water usage
  state.weeklyWater[6] = (int)state.todayWaterUsage;
  
  // Line chart - Temperature
  pota.dashboard.setX(widgets.tempTrendLine, hourLabels, 24);
  pota.dashboard.setY(widgets.tempTrendLine, state.tempHistory, 24);
  
  // Area chart - Humidity
  pota.dashboard.setX(widgets.humidityTrendArea, hourLabels, 24);
  pota.dashboard.setY(widgets.humidityTrendArea, state.humidityHistory, 24);
  
  // Bar chart - Weekly water
  pota.dashboard.setX(widgets.weeklyWaterBar, dayLabels, 7);
  pota.dashboard.setY(widgets.weeklyWaterBar, state.weeklyWater, 7);
  
  // Pie chart - Energy distribution
  pota.dashboard.setX(widgets.energyDistributionPie, state.energyLabels, 4);
  pota.dashboard.setY(widgets.energyDistributionPie, state.energyValues, 4);
  
  // Doughnut chart - Crop health
  pota.dashboard.setX(widgets.cropHealthDoughnut, state.healthLabels, 4);
  pota.dashboard.setY(widgets.cropHealthDoughnut, state.healthValues, 4);
}

// ════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   Smart Greenhouse Control System      ║");
  Serial.println("║      Complete Dashboard Demo           ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize POTA
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
  Serial.println("🌱 Greenhouse automation system ready!\n");
}

// ════════════════════════════════════════════════════════════
// LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  pota.loop();
  
  // Update sensors and dashboard periodically
  if (millis() - lastUpdate > UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Simulate sensor readings
    updateSensors();
    
    // Update dashboard cards
    pota.dashboard.setValue(widgets.tempCard, state.temperature);
    pota.dashboard.setValue(widgets.humidityCard, state.humidity);
    pota.dashboard.setValue(widgets.co2Card, state.co2Level);
    pota.dashboard.setValue(widgets.lightCard, state.lightIntensity);
    pota.dashboard.setValue(widgets.soilMoistureProgress, state.soilMoisture);
    
    // Update charts
    updateCharts();
    
    // Send updates
    pota.dashboard.sendUpdates();
    
    Serial.print("🌱 T:");
    Serial.print(state.temperature, 1);
    Serial.print("°C H:");
    Serial.print(state.humidity, 0);
    Serial.print("% CO2:");
    Serial.print(state.co2Level);
    Serial.print("ppm Soil:");
    Serial.print(state.soilMoisture);
    Serial.println("%");
  }
}
