/*
  POTA.h - Please Over The Air library header
  -------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air

  Description:
    Public API for the Please Over The Air (POTA) library.
    Provides a unified interface to:
      - Retrieve secure device identifiers (e.g. MAC address)
      - Initialize OTA client with Wi-Fi credentials or user-supplied client
      - Perform OTA update checks and apply firmware updates
      - Handle errors via POTAError codes and helper functions

  Usage:
    Include this header in your Arduino sketch:
      #include <POTA.h>

  Compatible boards:
    - ESP32 (using esp_https_ota)
    - ESP8266 (using ESPhttpUpdate)
    - Arduino Opta WiFi (using Arduino_Portenta_OTA)
*/

#pragma once



#ifdef ESP32
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #include <esp_mac.h> 
    #include <esp_https_ota.h>
#elif defined(ESP8266)
    #include <WiFiClientSecure.h>
    #include <ESP8266WiFi.h>
    #include <ESP8266httpUpdate.h>
    #include <CertStoreBearSSL.h>
#elif defined(ARDUINO_OPTA)
    #include <WiFi.h>
    #include <WiFiSSLClient.h>
    #include <Arduino_Portenta_OTA.h>
#else
    #error "Unsupported platform! Please compile for ESP32 or Arduino Opta."
#endif

/**
 * @brief Enum for all possible errors returned by POTA library functions.
 */
enum class POTAError {
    SUCCESS = 0,                    ///< Operation completed successfully
    PARAMETER_INVALID_SSID,         ///< SSID parameter is invalid
    PARAMETER_INVALID_PASSWORD,     ///< Password parameter is invalid
    PARAMETER_INVALID_DEVICETYPE,   ///< Device type parameter is invalid
    PARAMETER_INVALID_FWVERSION,    ///< Firmware version parameter is invalid
    PARAMETER_INVALID_AUTHTOKEN,    ///< Authentication token parameter is invalid
    PARAMETER_INVALID_SECRET,       ///< Secret key parameter is invalid
    PARAMETER_INVALID_OUTPUT,       ///< Output buffer is null or too small
    PARAMETER_INVALID_OTA_URL,      ///< OTA URL parameter is invalid
    WIFI_CONNECT_FAILED,            ///< Failed to connect to Wi-Fi
    CLIENT_NOT_INITIALIZED,         ///< Wi-Fi client was not initialized
    CONNECTION_FAILED,              ///< Could not connect to server
    JSON_PARSE_FAILED,              ///< Failed to parse JSON response
    TOKEN_GENERATION_FAILED,        ///< Failed to generate server token
    TOKEN_MISMATCH,                 ///< Server token did not match expected
    NO_UPDATE_AVAILABLE,            ///< No OTA update available
    OTA_FAILED,                     ///< OTA process failed (generic)
    OTA_DOWNLOAD_FAILED,            ///< OTA firmware download failed
    OTA_DECOMPRESSION_FAILED,       ///< OTA firmware decompression failed
    OTA_APPLY_FAILED,               ///< OTA firmware application failed
    OTA_NOT_CAPABLE,                ///< Portenta bootloader too old or not capable
    OTA_BEGIN_FAILED,               ///< OTA initialization failed
    OTA_WIFI_FW_MISSING,            ///< Arduino OPTA Wi-Fi firmware not installed
    PLATFORM_NOT_SUPPORTED,         ///< Board platform not supported
    BUFFER_OVERFLOW_REQUEST,        ///< Buffer overflow while building JSON request
    BUFFER_OVERFLOW_RESPONSE,      	///< Buffer overflow while reading server response
    CERTIFICATE_MISSING,             ///< Certificate not found in secure element
    SERVER_ERROR_4XX                ///< Server error code 4xx
};

/**
 * @brief Main class to handle secure OTA updates for ESP32 and Arduino Portenta (OPTA) boards.
 */
class POTA {
public:
    /**
     * @brief Default constructor.
     */
    POTA();
    
    /**
     * @brief Convert a POTAError enum into a human-readable string.
     * @param err The POTAError value
     * @return const char* describing the error
     */
    static const char* errorToString(POTAError err);
    
    /**
     * @brief Initialize Wi-Fi and POTA library with credentials and metadata.
     * @param ssid Wi-Fi SSID
     * @param password Wi-Fi password
     * @param deviceType Device type identifier
     * @param firmwareVersion Current firmware version
     * @param authToken Authentication token for server
     * @param serverSecret Secret key for generating server token
     * @return POTAError result of the initialization
     */
    POTAError begin(const char* ssid,
                    const char* password,
                    const char* deviceType,
                    const char* firmwareVersion,
                    const char* authToken,
                    const char* serverSecret);

    /**
     * @brief Initialize the library with an already connected client.
     */
#if defined(ESP32) || defined(ESP8266)
    POTAError beginClient(WiFiClientSecure& client,
                          const char* deviceType,
                          const char* firmwareVersion,
                          const char* authToken,
                          const char* serverSecret);
#elif defined(ARDUINO_OPTA)
    POTAError beginClient(WiFiSSLClient& client,
                          const char* deviceType,
                          const char* firmwareVersion,
                          const char* authToken,
                          const char* serverSecret);
#endif

    /**
     * @brief Check for available OTA update and perform it if available.
     * @return POTAError code indicating success or failure
     */
    POTAError checkAndPerformOTA();

    /**
     * @brief Get the unique, secure MAC address of the device.
     * @return MAC address as a String
     */
    String getSecureMACAddress();

private:
#if defined(ESP32) || defined(ESP8266)
    WiFiClientSecure* _client = nullptr;  ///< Pointer to ESP32/ESP8266 secure Wi-Fi client
#elif defined(ARDUINO_OPTA)
    WiFiSSLClient* _client = nullptr;     ///< Pointer to Portenta secure Wi-Fi client
#endif

    char _deviceType[32];        ///< Device type identifier
    char _firmwareVersion[32];   ///< Current firmware version
    char _authToken[64];         ///< Authentication token
    char _serverSecret[65];      ///< Secret key for server token generation

    /**
     * @brief Generate a secure token to verify OTA update from server.
     * @param update Whether an update is available
     * @param version Firmware version string
     * @param url OTA URL
     * @param checksum OTA firmware checksum
     * @param protocol_version Protocol version string
     * @param notes Release notes
     * @param timestamp Server timestamp
     * @param secret Secret key
     * @param outToken Output buffer for token
     * @param outTokenSize Size of output buffer
     * @return POTAError indicating success or failure
     */
    POTAError generateServerToken(bool update,
                                  const char* version,
                                  const char* url,
                                  const char* checksum,
                                  const char* protocol_version,
                                  const char* notes,
                                  const char* timestamp,
                                  const char* secret,
                                  char* outToken, size_t outTokenSize);

    /**
     * @brief Check the server for OTA update availability.
     * @param outOTAUrl Buffer to store OTA URL if update is available
     * @param outOTAUrlSize Size of output buffer
     * @return POTAError indicating success or type of failure
     */
    POTAError checkOTAUpdate(char* outOTAUrl, size_t outOTAUrlSize);

    /**
     * @brief Perform the OTA update using the provided URL.
     * @param OTA_file_url URL of the firmware to download
     * @return POTAError indicating success or type of failure
     */
    POTAError performOTA(const char* OTA_file_url);
    
};
