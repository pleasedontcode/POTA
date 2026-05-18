/*
  firmwareUpdateDashboard.ino - POTA Firmware Update Dashboard (Ultra-Simple)
  ------------------------------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/programming-over-the-air/

  Description:
    Ultra-simple firmware update manager with two update methods:
    
    1. POTA CLOUD UPDATE:
       - Automatic notification from server
       - Single button to install and restart
    
    2. MANUAL FILE UPLOAD:
       - Upload custom firmware binary
       - Get download URL
       - Single button to install via performOTA()

  Features:
    - Single tab interface
    - Two confirmation buttons
    - Real-time update notifications
    - Direct URL installation support

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

// ════════════════════════════════════════════════════════════
// WIDGET IDs
// ════════════════════════════════════════════════════════════
struct Widgets {
  uint8_t header;
  
  // POTA Cloud Update
  uint8_t currentVersion;
  uint8_t updateAvailable;
  uint8_t newVersion;
  uint8_t changelog;
  uint8_t installPotaButton;
  
  uint8_t separator;
  
  // Manual Upload
  uint8_t fileUpload;
  uint8_t uploadedUrl;
  uint8_t installManualButton;
} w;

// ════════════════════════════════════════════════════════════
// STATE
// ════════════════════════════════════════════════════════════
struct State {
  String currentVersion = FIRMWARE_VERSION;
  bool updateAvailable = false;
  String newVersion = "";
  String changelog = "";
  String uploadedUrl = "";
} state;

// ════════════════════════════════════════════════════════════
// POTA NOTIFICATION CALLBACK
// ════════════════════════════════════════════════════════════
void onFirmwareNotification(const char* version) {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   NEW FIRMWARE AVAILABLE!              ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.print("📦 New Version: ");
  Serial.println(version);
  Serial.print("📦 Current: ");
  Serial.println(state.currentVersion);
  
  // Update state
  state.updateAvailable = true;
  state.newVersion = String(version);
  state.changelog = "• Bug fixes\n• New features\n• Security patches";
  
  // Update dashboard
  pota.dashboard.setValue(w.updateAvailable, STATUS_WARNING);

  
  pota.dashboard.setValue(w.newVersion, "v" + state.newVersion);
  pota.dashboard.setValue(w.changelog, state.changelog);
  
  pota.dashboard.sendUpdates();
  
  Serial.println("\n✅ Check dashboard to install\n");
}

// ════════════════════════════════════════════════════════════
// DASHBOARD SETUP
// ════════════════════════════════════════════════════════════
void setupDashboard() {
  Serial.println("🎨 Setting up OTA Manager...");
  
  // Header
  w.header = pota.dashboard.addWidget(SEPARATOR_CARD, 
    "🔄 Firmware Update Manager", 
    0, 
    "Two update methods: POTA Cloud (automatic notification + restart) or Manual Upload (direct URL installation)");
  
  // ──── POTA CLOUD UPDATE ────
  w.currentVersion = pota.dashboard.addWidget(GENERIC_CARD, "Current Version", 0);
  w.updateAvailable = pota.dashboard.addWidget(FEEDBACK_CARD, "POTA Update", 0);
  w.newVersion = pota.dashboard.addWidget(GENERIC_CARD, "New Version", 0);
  w.changelog = pota.dashboard.addWidget(GENERIC_CARD, "Changelog", 0);
  w.installPotaButton = pota.dashboard.addWidget(ACTION_BUTTON_CARD, "Install POTA Update", 0, "Do you confirm to install new firmware from POTA cloud?");
  
  w.separator = pota.dashboard.addWidget(SEPARATOR_CARD, "📤 Manual Upload", 0);
  
  // ──── MANUAL UPLOAD ────
  w.fileUpload = pota.dashboard.addWidget(FILE_UPLOAD_CARD, "Upload Firmware", 0, ".bin,.ota");
  w.uploadedUrl = pota.dashboard.addWidget(GENERIC_CARD, "File URL", 0);
  w.installManualButton = pota.dashboard.addWidget(ACTION_BUTTON_CARD, "Install from URL", 0, "Do you confirm to install new firmware from URL?");
  
  Serial.println("✅ Widgets created");
  
  // Set initial values
  pota.dashboard.setValue(w.currentVersion, "v" + state.currentVersion);
  pota.dashboard.setValue(w.updateAvailable, STATUS_NONE);
  pota.dashboard.setValue(w.newVersion, "No updates");
  pota.dashboard.setValue(w.changelog, "N/A");
  pota.dashboard.setValue(w.uploadedUrl, "Waiting for upload...");
  
  setupCallbacks();
}

// ════════════════════════════════════════════════════════════
// CALLBACKS
// ════════════════════════════════════════════════════════════
void setupCallbacks() {
  Serial.println("🔗 Setting up callbacks...");
  
  // ──── INSTALL POTA UPDATE (with restart) ────
  pota.dashboard.onUpdate(w.installPotaButton, [](WidgetData data) {
    bool confirm = data.getBool();
    if (!confirm || !state.updateAvailable) {
      Serial.println("❌ No POTA update available!");
      return;
    }
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     INSTALLING POTA CLOUD UPDATE       ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("📦 Version: ");
    Serial.println(state.newVersion);
    Serial.println("🔄 Restarting device...");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    delay(1000);
    
    // Restart device - POTA will handle update at boot
    pota.restart();
  });
  
  // ──── FILE UPLOAD ────
  pota.dashboard.onUpdate(w.fileUpload, [](WidgetData data) {
    state.uploadedUrl = data.getString();
    
    Serial.println("\n📤 Firmware uploaded!");
    Serial.print("🔗 URL: ");
    Serial.println(state.uploadedUrl);
    
    pota.dashboard.setValue(w.uploadedUrl, state.uploadedUrl);
    pota.dashboard.setValue(w.updateAvailable, STATUS_WARNING);
    
    Serial.println("✅ Ready to install from URL");
  });
  
  // ──── INSTALL FROM MANUAL URL (with performOTA) ────
  pota.dashboard.onUpdate(w.installManualButton, [](WidgetData data) {
    if (state.uploadedUrl == "" || state.uploadedUrl == "Waiting for upload...") {
      Serial.println("❌ No file uploaded!");
      return;
    }
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     INSTALLING FROM MANUAL UPLOAD      ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("📦 URL: ");
    Serial.println(state.uploadedUrl);
    Serial.println("📥 Starting download and installation...");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Direct OTA installation from URL
    pota.performOTA(state.uploadedUrl.c_str());
  });
  
  Serial.println("✅ Callbacks ready");
}

// ════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║    OTA Firmware Manager (Simple)       ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.print("📦 Current Firmware: v");
  Serial.println(state.currentVersion);
  
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
    Serial.print("❌ POTA failed: ");
    Serial.println(POTA::errorToString(err));
    return;
  }
  
  Serial.println("✅ POTA initialized");
  
  // Register callbacks
  pota.onOTAAvailable(onFirmwareNotification);
  pota.dashboard.setWidgetConfigCallback(setupDashboard);

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
  
  Serial.println("✅ Setup complete\n");
  Serial.println("📱 Dashboard ready!\n");
}

// ════════════════════════════════════════════════════════════
// LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  pota.loop();
}
