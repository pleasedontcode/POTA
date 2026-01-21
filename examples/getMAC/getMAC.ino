/*
  getMAC.ino - Example for POTA library
  -------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    This example retrieves and prints the unique secure device
    MAC address using the POTA library. The MAC address is required
    when registering the device on the Please Over The Air (POTA)
    service at https://www.pleasedontcode.com, in order to enable
    remote OTA management for your firmware.

  Usage:
    - Upload this sketch to your device.
    - Open the Serial Monitor at 115200 baud.
    - Copy the printed MAC address.
    - Register the device with this MAC address on pleasedontcode.com
      to create its OTA project and manage future updates.

  Compatible boards:
    - ESP32
    - ESP8266
    - Arduino Opta WiFi
*/

#include "POTA.h"

POTA ota;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Print unique device MAC address
  String mac = ota.getSecureMACAddress();
  Serial.println("\nDevice MAC Address: " + mac);
}

void loop() {
  // Nothing to do here
}
