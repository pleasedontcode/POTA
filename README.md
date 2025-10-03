# POTA — Please Over The Air

POTA is an open-source library that simplifies remote firmware updates in a secure, simple and portable way for **ESP32**, **ESP8266**, and **Arduino Opta** devices. 
Integrated with the [POTA Dashboard](https://www.pleasedontcode.com/please-over-the-air/), it lets you register devices, upload OTA-ready firmware, and deploy updates with a single click.


## ✨ Features
- 🔐 Secure OTA with HMAC token verification  
- 📡 Works with ESP32, ESP8266, and Arduino Opta WiFi  
- 🌍 Integrated with [pleasedontcode.com/please-over-the-air/](https://www.pleasedontcode.com/please-over-the-air/) OTA service  
- ⚡ Easy setup with `secrets.h`  
- 📦 Lightweight and board-specific (uses `esp_https_ota`, `ESPhttpUpdate`, or `Arduino_Portenta_OTA`)  


## 📥 Installation

1. Download or clone this repository into your Arduino `libraries` folder:  git clone https://github.com/pleasedontcode/POTA.git
2. Restart the Arduino IDE.
3. The library will be available under **Sketch → Include Library → POTA**.


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
    - `POTA_Library_WiFi.ino` → POTA handles Wi-Fi connection automatically.
    - `POTA_User_WiFi.ino` → You manage Wi-Fi connection manually.
5. Run the sketch
    - The device will connect to Wi-Fi, initialize POTA, and perform a one-shot OTA check.
	
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
