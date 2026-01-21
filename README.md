# POTA — Please Over The Air

Secure, simple, and powerful library for **ESP32**, **ESP8266**, and **Arduino Opta** devices combining OTA firmware updates with real-time dashboard capabilities.  
POTA integrates with the [Please Over The Air](https://www.pleasedontcode.com/please-over-the-air/) service to deliver remote firmware updates, monitor device telemetry, and control your IoT devices through interactive web dashboards featuring 30+ customizable widgets (sliders, charts, buttons, and more).

## ✨ Features

### 🔐 Secure OTA Firmware Updates
- **HMAC-SHA256 Token Verification** — Every server response is cryptographically authenticated
- **TLS/SSL Encryption** — All communications use HTTPS with certificate validation
- **Platform-Specific Flash** — Native OTA libraries for optimal performance (`esp_https_ota`, `ESPhttpUpdate`, `Arduino_Portenta_OTA`)
- **Push Notifications** — Get instant alerts when new firmware is available via WebSocket
- **Automatic Version Management** — Track firmware versions and display release notes

### 📊 Real-Time Dashboard
- **30+ Widget Types** — Sliders, buttons, charts (bar/line/pie), gauges, joysticks, toggles, and more
- **Live Data Synchronization** — WebSocket-based bi-directional updates with automatic reconnection
- **Interactive Controls** — Receive user inputs from web dashboard with event callbacks
- **Customizable Charts** — Display sensor data with line, area, bar, pie, and doughnut charts
- **Tab Organization** — Group related widgets into multiple tabs for better UI organization
- **MessagePack Encoding** — Efficient binary protocol for minimal bandwidth usage

### 📡 Multi-Platform Support
- **ESP32** — All variants including DevKit, XIAO ESP32S3, Arduino Nano ESP32
- **ESP8266** — NodeMCU, Wemos D1 Mini, and compatible boards
- **Arduino Opta WiFi** — Industrial-grade Portenta-based platform
- **Automatic Wi-Fi Reconnection** — Built-in connection recovery on network disruptions

### 🌍 Cloud Integration
- **Seamless Portal Integration** — Connect to [pleasedontcode.com/please-over-the-air](https://www.pleasedontcode.com/please-over-the-air/) service
- **Device Management** — Register and manage multiple devices from web portal
- **Remote Monitoring** — View live device telemetry and status from anywhere
- **Firmware Repository** — Upload and manage firmware versions in the cloud

### ⚡ Developer-Friendly
- **Easy Setup** — Simple `secrets.h` configuration file
- **Minimal Dependencies** — Only requires ArduinoJson library
- **Type-Safe API** — Uses `std::variant` for type-safe data handling
- **Event-Driven Callbacks** — React to widget interactions and OTA notifications
- **Comprehensive Examples** — Ready-to-use examples for common scenarios
- **Lightweight Footprint** — Optimized memory usage for resource-constrained devices 


## 📥 Installation

### Method 1: Arduino Library Manager (Recommended)
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for **"POTA"**
4. Click **Install**

### Method 2: Manual Installation
1. Download the latest release from [GitHub](https://github.com/pleasedontcode/POTA/releases)
2. Extract to your Arduino `libraries` folder:
   - **Windows**: `Documents\Arduino\libraries\`
   - **macOS**: `~/Documents/Arduino/libraries/`
   - **Linux**: `~/Arduino/libraries/`
3. Restart Arduino IDE

### Method 3: Git Clone
```bash
cd ~/Arduino/libraries/
git clone https://github.com/pleasedontcode/POTA.git
```


## ⚙️ Getting Started with POTA

1. Upload `getMAC.ino` to your board
    - Open the Serial Monitor at 115200 baud.
    - Copy the printed MAC address.
2. Register your device on the POTA portal
    - Go to [https://www.pleasedontcode.com/please-over-the-air](https://www.pleasedontcode.com/please-over-the-air/).
    - Enter the MAC address to create a new OTA project.
    - The portal will provide your `AUTH_TOKEN` and `SERVER_SECRET`.
3. Set up `secrets.h`
    - Paste the `AUTH_TOKEN` and `SERVER_SECRET` obtained from the portal.
    - Update your Wi-Fi credentials (`WIFI_SSID` and `WIFI_PASSWORD`).
    - Set your `DEVICE_TYPE` (e.g., `ESP32_DEVKIT_V1`).
    - Set your `FIRMWARE_VERSION` (e.g., `"01.00.00"`).
4. Upload one of the POTA examples
   
    **Firmware Update Examples:**
   - `firmwareUpdate.ino` → Safe OTA updates with WebSocket notifications and boot-time checks
   - `firmwareManagerDashboard.ino` → Interactive OTA manager with POTA Cloud and manual upload support
     
   **Dashboard Examples:**
   - `simpleDashboard.ino` → Basic sensor display with real-time data cards
   - `interactiveDashboard.ino` → Control widgets with sliders and toggles (demonstrates int/float precision)
   - `chartsDashboard.ino` → Data visualization with correlated temperature, energy, and pie charts
   - `fullDashboard.ino` → Complete smart greenhouse automation system (all 33 widgets)
6. Run the sketch
   - The device will connect to Wi-Fi and initialize POTA
   - Firmware update examples will check for available updates at boot
   - Dashboard examples will create widgets and start real-time monitoring
  
[![Getting Started with Please Over The Air](https://img.youtube.com/vi/FxSQxyJAVsU/0.jpg)](https://www.youtube.com/watch?v=FxSQxyJAVsU "Getting Started with Please Over The Air")
[![Registering Your Device on the POTA Dashboard](https://img.youtube.com/vi/aC1VmWriOm0/0.jpg)](https://www.youtube.com/watch?v=aC1VmWriOm0 "Registering Your Device on the POTA Dashboard")
[![Your first OTA Update](https://img.youtube.com/vi/u2OzN_Ubm_A/0.jpg)](https://www.youtube.com/watch?v=u2OzN_Ubm_A "Your first OTA Update")

## 📊 Dashboard Widgets

POTA supports 30+ widget types organized into categories:

### Display Widgets (Read-Only)
- `GENERIC_CARD` — Customizable display card
- `TEMPERATURE_CARD` — Temperature with icon
- `HUMIDITY_CARD` — Humidity percentage
- `AIR_CARD` — Air quality indicator
- `ENERGY_CARD` — Power/energy consumption
- `FEEDBACK_CARD` — Status feedback with color states
- `PROGRESS_CARD` — Progress bar (0-100%)

### Control Widgets (User Input)
- `TOGGLE_BUTTON_CARD` — On/Off switch
- `SLIDER_CARD` — Numeric slider
- `PUSH_BUTTON_CARD` — Momentary button
- `ACTION_BUTTON_CARD` — Trigger button (no state)
- `RANGE_SLIDER_CARD` — Dual-handle range selector
- `DROPDOWN_CARD` — Dropdown selection menu
- `TEXT_INPUT_CARD` — Text input field
- `PASSWORD_CARD` — Password input (masked)
- `COLOR_PICKER_CARD` — Color selection
- `WEEK_SELECTION_CARD` — Day-of-week selector
- `INDICATOR_BUTTON_CARD` — Button with state indicator

### Joystick Widgets
- `JOYSTICK_2D_CARD` — 2D joystick (X+Y)
- `JOYSTICK_X_CARD` — Horizontal joystick
- `JOYSTICK_Y_CARD` — Vertical joystick

### Chart Widgets
- `BAR_CHART` — Vertical/horizontal bars
- `LINE_CHART` — Line graph
- `AREA_CHART` — Filled area graph
- `PIE_CHART` — Pie chart
- `DOUGHNUT_CHART` — Doughnut chart

### Special Widgets
- `TAB` — Container for organizing widgets
- `SEPARATOR_CARD` — Visual divider
- `LINK_CARD` — Clickable hyperlink
- `IMAGE_CARD` — Image display
- `FILE_UPLOAD_CARD` — File upload interface
- `TIME_SYNC_CARD` — Time synchronization widget
- `TIME_DIFFERENCE_CARD` — Time difference calculator
	
## 🛡 Security

- Each device is uniquely identified by its secure MAC address.
- OTA update requests are validated with HMAC authentication.
- Firmware is delivered securely via HTTPS.
	
## 🧩 Supported Boards

- ESP32-based boards (e.g. DevKit, XIAO ESP32S3, Arduino Nano ESP32)
- ESP8266-based boards (NodeMCU v1.0, Wemos D1 Mini, etc.)
- Arduino Opta WiFi

More boards will be added soon.
	
##  📄 License

This project is licensed under the MIT License. See LICENSE for details.

## 🌐 Links

📘 Documentation: [https://www.pleasedontcode.com/please-over-the-air](https://www.pleasedontcode.com/please-over-the-air/)

🛠 Repository: [https://github.com/pleasedontcode/POTA.git](https://github.com/pleasedontcode/POTA.git)

✉️ Author: Francesco Alessandro Colucci — info@pleasedontcode.com
