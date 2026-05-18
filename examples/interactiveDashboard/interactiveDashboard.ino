/*
  interactiveDashboard.ino - POTA Dashboard Interactive Example
  --------------------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/programming-over-the-air/

  Description:
    Interactive dashboard example showing bidirectional control.
    Learn how to handle user inputs and control devices remotely.
    
    Features:
    - Toggle switches for device control
    - Integer slider (volume) with step = 1
    - Float slider (brightness) with step = 0.1
    - Color picker for RGB LED control
    - Reset button to restore defaults
    - Callbacks for user interactions
    - Status feedback indicators

  Scenario:
    Smart home lighting and audio control system

  Widgets Used:
    - TOGGLE_BUTTON_CARD: On/Off switches
    - SLIDER_CARD (int): Volume control with integer values
    - SLIDER_CARD (float): Brightness control with decimal precision
    - COLOR_PICKER_CARD: RGB color selection
    - ACTION_BUTTON_CARD: Reset to defaults
    - FEEDBACK_CARD: Status indicators
    
  Step Parameter Details:
    The 'step' parameter in SLIDER_CARD controls both increment and precision:
    - Integer step (1, 5, 10): Values received as int (whole numbers)
    - Float step (0.1): Values with 1 decimal place (12.3, 45.6)
    - Float step (0.01): Values with 2 decimal places (12.34, 45.67)
    - Float step (0.00005): Values with 5 decimal places (1.23456, 7.89012)
    
    The decimal precision is automatically determined by the step value.

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
uint8_t ledToggle;
uint8_t volumeSlider;
uint8_t brightnessSlider;
uint8_t colorPicker;
uint8_t resetButton;
uint8_t statusFeedback;

// Device state
bool ledState = false;
int volume = 50;              // Integer value (step = 1)
float brightness = 75.0;      // Float value (step = 0.1)
String ledColor = "#FF5733";

// Initial values for reset
const bool INITIAL_LED_STATE = false;
const int INITIAL_VOLUME = 50;
const float INITIAL_BRIGHTNESS = 75.0;
const String INITIAL_COLOR = "#FF5733";

/**
 * @brief Configure all dashboard widgets
 */
void setupDashboard() {
  Serial.println("🎨 Setting up interactive dashboard...");
  
  // Create header separator with instructions
  headerSeparator = pota.dashboard.addWidget(SEPARATOR_CARD, "💡 Smart Home Control Panel", 0, "Control your devices remotely: toggle LED on/off, adjust volume (integer steps) and brightness (float precision), pick colors for RGB lighting. Use Reset button to restore default settings");
  
  // Create control widgets
  ledToggle = pota.dashboard.addWidget(TOGGLE_BUTTON_CARD, "LED Control");
  
  // Volume slider with INTEGER step
  // Step = 1 (integer) → values received as int: 0, 1, 2, 3, etc.
  // The step parameter defines both increment size AND value precision:
  // - Integer step (1, 5, 10) → integer values
  // - Float step (0.1, 0.01, 0.001) → float values with matching decimal precision
  // Example: step = 0.01 → values always have 2 decimals (12.34, 56.78)
  //          step = 0.00005 → values have 5 decimals (1.23456, 7.89012)
  volumeSlider = pota.dashboard.addWidget(SLIDER_CARD, "Volume", 0, "", 1, 0, 100);
  
  // Brightness slider with FLOAT step
  // Step = 0.1 (one decimal) → values received as float: 0.0, 0.1, 0.2, etc.
  brightnessSlider = pota.dashboard.addWidget(SLIDER_CARD, "Brightness", 0, "", 0.1, 0, 100);
  
  colorPicker = pota.dashboard.addWidget(COLOR_PICKER_CARD, "LED Color");
  resetButton = pota.dashboard.addWidget(ACTION_BUTTON_CARD, "Reset to Defaults");
  statusFeedback = pota.dashboard.addWidget(FEEDBACK_CARD, "System Status");
  
  // Set initial values
  pota.dashboard.setValue(ledToggle, ledState);
  pota.dashboard.setValue(volumeSlider, volume);
  pota.dashboard.setValue(brightnessSlider, brightness);
  pota.dashboard.setValue(colorPicker, ledColor);
  pota.dashboard.setValue(statusFeedback, STATUS_SUCCESS);
  
  // Register callbacks
  setupCallbacks();
  
  Serial.println("✅ Dashboard configured!");
}

/**
 * @brief Register widget interaction callbacks
 */
void setupCallbacks() {
  // LED toggle callback
  pota.dashboard.onUpdate(ledToggle, [](WidgetData data) {
    ledState = data.getBool();
    
    Serial.print("💡 LED turned ");
    Serial.println(ledState ? "ON" : "OFF");
    
    // Control actual LED
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    
    // Update status
    pota.dashboard.setValue(statusFeedback, ledState ? STATUS_SUCCESS : STATUS_NONE);
  });
  
  // Volume slider callback (INTEGER values)
  // With step = 1, values are received as integers (0, 1, 2, ..., 100)
  pota.dashboard.onUpdate(volumeSlider, [](WidgetData data) {
    volume = data.getInt();
    
    Serial.print("🔊 Volume set to: ");
    Serial.print(volume);
    Serial.println("%");
    
    // In a real application, control audio output:
    // setAudioVolume(volume);
  });
  
  // Brightness slider callback (FLOAT values)
  // With step = 0.1, values are received as floats with 1 decimal (0.0, 0.1, 0.2, ..., 100.0)
  pota.dashboard.onUpdate(brightnessSlider, [](WidgetData data) {
    brightness = data.getFloat();
    
    Serial.print("🔆 Brightness set to: ");
    Serial.print(brightness, 1);  // Print with 1 decimal place
    Serial.println("%");
    
    // In a real application, control PWM output:
    // analogWrite(LED_PIN, map(brightness * 10, 0, 1000, 0, 255));
  });
  
  // Color picker callback
  pota.dashboard.onUpdate(colorPicker, [](WidgetData data) {
    ledColor = data.getString();
    
    Serial.print("🎨 Color selected: ");
    Serial.println(ledColor);
    
    // In a real application, parse hex color and set RGB LED:
    // parseHexColor(ledColor);
    // setRGBLed(r, g, b);
    
    pota.dashboard.setValue(statusFeedback, STATUS_INFO);
  });
  
  // Reset button callback
  pota.dashboard.onUpdate(resetButton, [](WidgetData data) {
    Serial.println("🔄 Resetting to default values...");
    
    // Restore initial values
    ledState = INITIAL_LED_STATE;
    volume = INITIAL_VOLUME;
    brightness = INITIAL_BRIGHTNESS;
    ledColor = INITIAL_COLOR;
    
    // Update hardware
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    
    // Update dashboard widgets
    pota.dashboard.setValue(ledToggle, ledState);
    pota.dashboard.setValue(volumeSlider, volume);
    pota.dashboard.setValue(brightnessSlider, brightness);
    pota.dashboard.setValue(colorPicker, ledColor);
    pota.dashboard.setValue(statusFeedback, STATUS_NONE);
    
    Serial.println("✅ Reset complete!");
  });
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Configure LED pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  POTA Interactive Dashboard Example    ║");
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
  Serial.println("📱 Open dashboard to control the device!\n");
}

void loop() {
  pota.loop();
  
  // Your application code here
  // The dashboard handles all interactions through callbacks
}
