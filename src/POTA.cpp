/*
  POTA.cpp - Please Over The Air library implementation
  -----------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air/

  Description:
    Implementation of the Please Over The Air (POTA) library.
    Contains all logic for:
      - Secure MAC address retrieval
      - Wi-Fi initialization and OTA client setup
      - Communicating with the POTA update service
      - Verifying server HMAC token before firmware update
      - Performing board-specific OTA update mechanisms

  Notes:
    This file is part of the POTA library. The library relies on
    the ArduinoJson and board-specific OTA libraries (esp_https_ota, 
    ESPhttpUpdate, Arduino_Portenta_OTA).

  See also:
    POTA.h for API declarations and usage in sketches.
*/

#include "POTA.h"
#include "certificates.h"
#include <ArduinoJson.h>
#if defined(ARDUINO_OPTA)
    #include "opta_info.h"
    #include <mbedtls/md.h>
#endif
#if defined(ESP32)
    #include <esp_idf_version.h> 
    #include <mbedtls/md.h>
#endif

#define POTA_PROTOCOL_VERSION "01.00"
#define API_HOST "www.pleasedontcode.com"
#define CHECK_UPDATE_API "/api/v1/check_update/"

// -------------------- Constructor --------------------
POTA::POTA() {
    _client = nullptr;
    _deviceType[0] = '\0';
    _firmwareVersion[0] = '\0';
    _authToken[0] = '\0';
    _serverSecret[0] = '\0';
}

// -------------------- Public API --------------------
POTAError POTA::begin(const char* ssid,
                      const char* password,
                      const char* deviceType,
                      const char* firmwareVersion,
                      const char* authToken,
                      const char* serverSecret) 
{
    // Validate WiFi input parameters
    if (!ssid || strlen(ssid) == 0) 
        return POTAError::PARAMETER_INVALID_SSID;
    
    if (!password || strlen(password) == 0) 
        return POTAError::PARAMETER_INVALID_PASSWORD;
    
	 // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    Serial.print("📶 Connecting to Wi-Fi: ");
    Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 30000) return POTAError::WIFI_CONNECT_FAILED; // 30s timeout
    }
    Serial.println("\n✅ Wi-Fi connected, IP: " + WiFi.localIP().toString());

#if defined(ESP32) || defined(ESP8266)
    static WiFiClientSecure client;
    return beginClient(client, deviceType, firmwareVersion, authToken, serverSecret);
#elif defined(ARDUINO_OPTA)
    static WiFiSSLClient client;
    return beginClient(client, deviceType, firmwareVersion, authToken, serverSecret);
#else
    return POTAError::PLATFORM_NOT_SUPPORTED;
#endif
}

#if defined(ESP32) || defined(ESP8266)
POTAError POTA::beginClient(WiFiClientSecure& client,
#elif defined(ARDUINO_OPTA)
POTAError POTA::beginClient(WiFiSSLClient& client,
#endif
                            const char* deviceType,
                            const char* firmwareVersion,
                            const char* authToken,
                            const char* serverSecret)
{
    // Validate input parameters
    if (!deviceType || strlen(deviceType) == 0 || strlen(deviceType) >= sizeof(_deviceType)) 
        return POTAError::PARAMETER_INVALID_DEVICETYPE;
    
    if (!firmwareVersion || strlen(firmwareVersion) == 0 || strlen(firmwareVersion) >= sizeof(_firmwareVersion)) 
        return POTAError::PARAMETER_INVALID_FWVERSION;
    
    if (!authToken || strlen(authToken) == 0 || strlen(authToken) >= sizeof(_authToken)) 
        return POTAError::PARAMETER_INVALID_AUTHTOKEN;
    
    if (!serverSecret || strlen(serverSecret) == 0 || strlen(serverSecret) >= sizeof(_serverSecret)) 
        return POTAError::PARAMETER_INVALID_SECRET;

    _client = &client;

    strncpy(_deviceType, deviceType, sizeof(_deviceType) - 1);
    _deviceType[sizeof(_deviceType) - 1] = '\0';

    strncpy(_firmwareVersion, firmwareVersion, sizeof(_firmwareVersion) - 1);
    _firmwareVersion[sizeof(_firmwareVersion) - 1] = '\0';

    strncpy(_authToken, authToken, sizeof(_authToken) - 1);
    _authToken[sizeof(_authToken) - 1] = '\0';

    strncpy(_serverSecret, serverSecret, sizeof(_serverSecret) - 1);
    _serverSecret[sizeof(_serverSecret) - 1] = '\0';

    return POTAError::SUCCESS;
}

// -------------------- Public Methods --------------------
String POTA::getSecureMACAddress() {
#if defined(ESP32)
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
#elif defined(ESP8266)
    return WiFi.macAddress();
#elif defined(ARDUINO_OPTA)
    OptaBoardInfo *info;
    OptaBoardInfo* boardInfo();
    info = boardInfo();
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             info->mac_address[0], info->mac_address[1], info->mac_address[2],
             info->mac_address[3], info->mac_address[4], info->mac_address[5]);
    return String(macStr);
#else
    return String("ERROR_PLATFORM_NOT_SUPPORTED - UNKNOWN_MAC");
#endif
}

// -------------------- OTA Handling --------------------
POTAError POTA::checkAndPerformOTA() {
    if (!_client) return POTAError::CLIENT_NOT_INITIALIZED;

    char otaUrl[256];
    POTAError err = checkOTAUpdate(otaUrl, sizeof(otaUrl));
    if (err != POTAError::SUCCESS) return err;

    return performOTA(otaUrl);
}

// -------------------- Internal Helpers --------------------
POTAError POTA::generateServerToken(bool update,
                                    const char* version,
                                    const char* url,
                                    const char* checksum,
                                    const char* protocol_version,
                                    const char* notes,
                                    const char* timestamp,
                                    const char* secret,
                                    char* outToken, size_t outTokenSize)
{
    if (!secret) return POTAError::PARAMETER_INVALID_SECRET;
    if (!outToken || outTokenSize < 65) return POTAError::PARAMETER_INVALID_OUTPUT;

    char message[512];
    int n = snprintf(message, sizeof(message), "%s:%s:%s:%s:%s:%s:%s",
                     update ? "true" : "false",
                     version ? version : "",
                     url ? url : "",
                     checksum ? checksum : "",
                     protocol_version ? protocol_version : "",
                     notes ? notes : "",
                     timestamp ? timestamp : "");
    if (n < 0 || n >= (int)sizeof(message)) return POTAError::TOKEN_GENERATION_FAILED;

    unsigned char hmac[32]; // SHA256 produce 32 byte
    
    #if defined(ESP32) || defined(ARDUINO_OPTA)
        mbedtls_md_context_t ctx;
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, info, 1);
        mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
        mbedtls_md_hmac_update(&ctx, (const unsigned char*)message, strlen(message));
        mbedtls_md_hmac_finish(&ctx, hmac);
        mbedtls_md_free(&ctx);
    #elif defined(ESP8266)
        br_hmac_key_context kc;
        br_hmac_context ctx;
        br_hmac_key_init(&kc, &br_sha256_vtable, secret, strlen(secret));
        br_hmac_init(&ctx, &kc, 32);  // 32 = output size SHA256
        br_hmac_update(&ctx, message, strlen(message));
        br_hmac_out(&ctx, hmac);
    #endif

    static const char hexChars[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        outToken[i*2]     = hexChars[(hmac[i] >> 4) & 0x0F];
        outToken[i*2 + 1] = hexChars[hmac[i] & 0x0F];
    }
    outToken[64] = '\0';
    return POTAError::SUCCESS;
}

POTAError POTA::checkOTAUpdate(char* outOTAUrl, size_t outOTAUrlSize) {
    // Validate inputs
    if (!_client) return POTAError::CLIENT_NOT_INITIALIZED;
    if (!outOTAUrl || outOTAUrlSize == 0) return POTAError::PARAMETER_INVALID_OUTPUT;
    outOTAUrl[0] = '\0';

    #if defined(ESP32)
        // Set Root CA for secure TLS connection (ESP32 only)
        _client->setCACert(root_ca);
    #endif
    
    #if defined(ESP8266)
        static X509List cert(root_ca);
        _client->setTrustAnchors(&cert);
    #endif
    
    #if defined(ARDUINO_OPTA)
        _client->appendCustomCACert(root_ca);
    #endif

    // Try to connect to the OTA server
    if (!_client->connect(API_HOST, 443)) return POTAError::CONNECTION_FAILED;
    Serial.println("🔗 Connected to server");

    // Single buffer used for both request JSON body and server response
    char buffer[1024];

    // --- Build JSON request body ---
    int bodyLen = snprintf(buffer, sizeof(buffer),
             "{"
             "\"device_id\":\"%s\","
             "\"device_type\":\"%s\","
             "\"firmware_version\":\"%s\","
             "\"protocol_version\":\"%s\","
             "\"auth_token\":\"%s\""
             "}",
             getSecureMACAddress().c_str(),
             _deviceType,
             _firmwareVersion,
             POTA_PROTOCOL_VERSION,
             _authToken);

    // Check for buffer overflow during request construction
    if (bodyLen < 0 || bodyLen >= (int)sizeof(buffer)) {
        Serial.println("❌ BUFFER_OVERFLOW_REQUEST while building JSON request");
        _client->stop();
        return POTAError::BUFFER_OVERFLOW_REQUEST;
    }

    // --- Send HTTP POST request ---
    _client->println("POST " CHECK_UPDATE_API " HTTP/1.1");
    _client->println("Host: " API_HOST);
    _client->println("Content-Type: application/json");
    _client->print("Content-Length: ");
    _client->println(bodyLen);
    _client->println("Connection: close");
    _client->println();
    _client->println(buffer); // Send JSON body

    // Wait until server starts responding
    while (_client->connected() && !_client->available()) delay(10);

    // --- Skip HTTP headers ---
    while (_client->available()) {
        char line[128];
        size_t len = _client->readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if (strcmp(line, "\r") == 0) break; // End of headers
    }

    // --- Read HTTP response body into same buffer ---
    size_t len = _client->readBytes(buffer, sizeof(buffer) - 1);
    
    // Check for buffer overflow during response read
    if (len >= sizeof(buffer) - 1) {
        Serial.println("❌ BUFFER_OVERFLOW_RESPONSE while reading server response");
        _client->stop();
        return POTAError::BUFFER_OVERFLOW_RESPONSE;
    }

    buffer[len] = '\0'; // Null terminate string
    
    _client->stop();
    Serial.println("🔌 Disconnected from server");

    // --- Parse JSON response ---
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, buffer);
    if (error) {
        Serial.print("❌ JSON parse failed: ");
        Serial.println(error.c_str());
        return POTAError::JSON_PARSE_FAILED;
    }

    // Extract OTA metadata fields
    bool update = doc["update"] | false;
    const char* url = doc["url"] | "";
    const char* version = doc["version"] | "";
    const char* checksum = doc["checksum"] | "";
    const char* protocol_version = doc["protocol_version"] | "";
    const char* notes = doc["notes"] | "";
    const char* server_token = doc["server_token"] | "";
    const char* errorMsg = doc["error"] | "";
    long timestampValue = doc["timestamp"] | 0;
    

    // Convert timestamp into string for token generation
    char timestampStr[32];
    snprintf(timestampStr, sizeof(timestampStr), "%ld", timestampValue);

    if (strlen(errorMsg) > 0) {
        Serial.print("❌ Server error message: ");
        Serial.println(errorMsg);
        return POTAError::SERVER_ERROR_4XX;
    }

    // --- Verify server token for security ---
    char expectedToken[65];
    POTAError err = generateServerToken(update, version, url, checksum,
                                        protocol_version, notes, timestampStr,
                                        _serverSecret, expectedToken, sizeof(expectedToken));
    if (err != POTAError::SUCCESS) return err;

    // Compare expected vs received token
    if (strcmp(expectedToken, server_token) != 0) return POTAError::TOKEN_MISMATCH;

    // --- If update is available and URL is valid ---
    if (update && strncmp(url, "https://" API_HOST, strlen("https://" API_HOST)) == 0) {
        Serial.print("⬆️ New firmware version available: ");
        Serial.println(version);
        Serial.print("📝 Notes: ");
        Serial.println(notes);
        strncpy(outOTAUrl, url, outOTAUrlSize - 1);
        outOTAUrl[outOTAUrlSize - 1] = '\0'; // Ensure null-termination
        return POTAError::SUCCESS;
    }

    // Otherwise, no update available
    return POTAError::NO_UPDATE_AVAILABLE;
}

POTAError POTA::performOTA(const char* OTA_file_url) {
    // Validate OTA URL
    if (!OTA_file_url || strlen(OTA_file_url) == 0) 
        return POTAError::PARAMETER_INVALID_OTA_URL;

#if defined(ESP32)
    // ESP32 OTA using esp_https_ota
    Serial.println("🔍 Checking for OTA update...");
    esp_http_client_config_t http_config = {
        .url = OTA_file_url,
        .cert_pem = root_ca,
        .timeout_ms = 10000,
    };
    
    #if ESP_IDF_VERSION_MAJOR < 5
        // ESP-IDF v4.x → core Arduino ESP32 2.x
        esp_err_t ret = esp_https_ota(&http_config);
    #else
        // ESP-IDF v5.x → core Arduino ESP32 3.x
        esp_https_ota_config_t ota_config = {
            .http_config = &http_config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
    #endif
    
    if (ret == ESP_OK) {
        Serial.println("✅ OTA update completed. Restarting...");
        esp_restart();
        return POTAError::SUCCESS;
    } else {
        Serial.printf("❌ OTA failed. Error: %s\n", esp_err_to_name(ret));
        return POTAError::OTA_FAILED;
    }
    
#elif defined(ESP8266)
    // ESP8266 OTA using ESP8266httpUpdate
    Serial.println("🔍 Checking for OTA update...");
    t_httpUpdate_return ret = ESPhttpUpdate.update(*_client, String(OTA_file_url));
    if (ret == HTTP_UPDATE_FAILED) {
        Serial.printf("❌ OTA failed. Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        return POTAError::OTA_FAILED;
    }
    Serial.println("✅ OTA update completed. Restarting...");
    return POTAError::SUCCESS;

#elif defined(ARDUINO_OPTA)
    // Portenta OTA using Arduino_Portenta_OTA
    Serial.println("🔍 Checking for OTA update...");

    // Initialize OTA object
    Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);
    if (!ota.isOtaCapable()) return POTAError::OTA_NOT_CAPABLE;

    Arduino_Portenta_OTA::Error err;
    if ((err = ota.begin()) != Arduino_Portenta_OTA::Error::None) 
        return POTAError::OTA_BEGIN_FAILED;

    // Download OTA firmware
    Serial.println("⬇️ Starting OTA firmware download...");
    int downloaded = ota.download(OTA_file_url, true);
    Serial.print("⬇️ Download result: ");
    Serial.println(downloaded);
    if (downloaded == -3011) return POTAError::OTA_WIFI_FW_MISSING;
    if (downloaded <= 0) return POTAError::OTA_DOWNLOAD_FAILED;
    Serial.println("✅ OTA firmware downloaded successfully.");

    // Decompress OTA firmware
    Serial.println("🗜️ Decompressing OTA firmware...");
    int decompressed = ota.decompress();
    Serial.print("🗜️ Decompression result: ");
    Serial.println(decompressed);
    if (decompressed <= 0) return POTAError::OTA_DECOMPRESSION_FAILED;
    Serial.println("✅ OTA firmware decompressed successfully.");

    // Apply OTA update
    Serial.println("⚡ Applying OTA update...");
    if ((err = ota.update()) != Arduino_Portenta_OTA::Error::None) 
        return POTAError::OTA_APPLY_FAILED;

    Serial.println("✅ OTA update completed. Restarting...");
    delay(1000);
    ota.reset();
    return POTAError::SUCCESS;

#endif
}

const char* POTA::errorToString(POTAError err) {
    switch (err) {
        case POTAError::SUCCESS: return "SUCCESS";
        case POTAError::PARAMETER_INVALID_SSID: return "Invalid SSID parameter";
        case POTAError::PARAMETER_INVALID_PASSWORD: return "Invalid Wi-Fi password parameter";
        case POTAError::PARAMETER_INVALID_DEVICETYPE: return "Invalid device type parameter";
        case POTAError::PARAMETER_INVALID_FWVERSION: return "Invalid firmware version parameter";
        case POTAError::PARAMETER_INVALID_AUTHTOKEN: return "Invalid authentication token parameter";
        case POTAError::PARAMETER_INVALID_SECRET: return "Invalid secret key parameter";
        case POTAError::PARAMETER_INVALID_OUTPUT: return "Output buffer is null or too small";
        case POTAError::PARAMETER_INVALID_OTA_URL: return "Invalid OTA URL parameter";
        case POTAError::WIFI_CONNECT_FAILED: return "Failed to connect to Wi-Fi";
        case POTAError::CLIENT_NOT_INITIALIZED: return "Wi-Fi client not initialized";
        case POTAError::CONNECTION_FAILED: return "Could not connect to server";
        case POTAError::JSON_PARSE_FAILED: return "Failed to parse JSON response";
        case POTAError::TOKEN_GENERATION_FAILED: return "Failed to generate server token";
        case POTAError::TOKEN_MISMATCH: return "Server token did not match expected";
        case POTAError::NO_UPDATE_AVAILABLE: return "No OTA update available";
        case POTAError::OTA_FAILED: return "OTA process failed (generic)";
        case POTAError::OTA_DOWNLOAD_FAILED: return "OTA firmware download failed";
        case POTAError::OTA_DECOMPRESSION_FAILED: return "OTA firmware decompression failed";
        case POTAError::OTA_APPLY_FAILED: return "OTA firmware application failed";
        case POTAError::OTA_NOT_CAPABLE: return "Portenta bootloader too old or not capable";
        case POTAError::OTA_BEGIN_FAILED: return "OTA initialization failed";
        case POTAError::PLATFORM_NOT_SUPPORTED: return "Board platform not supported";
        case POTAError::BUFFER_OVERFLOW_REQUEST: return "Buffer overflow while building JSON request";
        case POTAError::BUFFER_OVERFLOW_RESPONSE: return "Buffer overflow while reading server response";
        case POTAError::OTA_WIFI_FW_MISSING: return "Wi-Fi firmware not installed. Please run WifiFirmwareUpdater.ino / QSPIFormat.ino at least once before performing OTA.";
        case POTAError::SERVER_ERROR_4XX: return "Server returned a 4xx error";
        default: return "Undefined error";
    }
}