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
      - Secure MAC address retrieval (platform-specific)
      - Wi-Fi initialization and OTA client setup
      - Communicating with POTA update service via HTTPS
      - Verifying server responses with HMAC-SHA256 tokens
      - Performing board-specific OTA update mechanisms
      - Managing WebSocket connections for real-time dashboard

  Security:
    - All API communications use TLS/SSL encryption
    - Server responses verified with HMAC-SHA256 signatures
    - WebSocket connections authenticated with JWT tokens
    - Root CA certificate validation for HTTPS connections

  Notes:
    This file is part of the POTA library. The library relies on:
    - ArduinoJson (JSON parsing and MessagePack encoding)
    - Platform-specific OTA libraries (esp_https_ota, ESPhttpUpdate, Arduino_Portenta_OTA)
    - Platform-specific crypto libraries (mbedTLS, BearSSL)

  See also:
    POTA.h for API declarations and usage examples
*/

#include "POTA.h"
#include "certificates.h"
#include <ArduinoJson.h>

// ========================================
// PLATFORM-SPECIFIC INCLUDES
// ========================================
#ifdef ESP32
    #include <esp_mac.h>          // For secure MAC address retrieval
    #include <esp_https_ota.h>    // For OTA update functionality
    #include <esp_idf_version.h>  // For ESP-IDF version detection
    #include <mbedtls/md.h>       // For HMAC-SHA256 calculation

    // Use NuSock library (same as ESP8266/Opta) to avoid heap fragmentation
    // from WebSocketsClient's internal SSL context allocation
    #define WS_CLIENT_TYPE NuSockClient
    
#elif defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266httpUpdate.h>  // For OTA update functionality
    #include <WiFiClientSecure.h>   // For SSL client
    
    // Use NuSock library instead of WebSocketsClient for better SSL stability
    #define WS_CLIENT_TYPE NuSockClient
    #define WIFI_CLIENT_TYPE WiFiClientSecure
    
#elif defined(ARDUINO_OPTA)
    #include <Arduino_Portenta_OTA.h> // For OTA update functionality
    #include "opta_info.h"            // For board information (MAC address)
    #include <mbedtls/md.h>           // For HMAC-SHA256 calculation
    
    #define WS_CLIENT_TYPE NuSockClient
    
#else
    #error "Unsupported platform! Please compile for ESP32, ESP8266 or Arduino Opta."
#endif

// ========================================
// GLOBAL STATIC INSTANCES
// ========================================
static WS_CLIENT_TYPE* _ws     = nullptr; ///< WebSocket client instance

// ========================================
// API CONFIGURATION
// ========================================
#define POTA_PROTOCOL_VERSION "01.00"           ///< Current POTA protocol version
#define API_HOST "www.pleasedontcode.com"       ///< POTA API server hostname
#define CHECK_UPDATE_API "/api/v1/check_update/" ///< OTA check endpoint
#define GET_WS_TOKEN_API "/api/v1/get_ws_token/" ///< WebSocket token endpoint

// ========================================
// CONSTRUCTOR
// ========================================
/**
 * @brief Constructs POTA instance and initializes dashboard send handler
 * 
 * The constructor:
 * 1. Initializes all member variables to safe defaults
 * 2. Registers a lambda function to bridge dashboard MessagePack output
 *    to WebSocket binary transmission
 * 
 * The send handler automatically selects the correct WebSocket method
 * based on platform (sendBIN for ESP, send for Opta).
 */
POTA::POTA() {
    _deviceType[0] = '\0';
    _firmwareVersion[0] = '\0';
    _authToken[0] = '\0';
    _serverSecret[0] = '\0';
    
    // Register dashboard send handler
    // This lambda is called whenever dashboard needs to transmit data
    // All platforms use NuSock library with send() for binary data
    _dashboard->setSendHandler([](uint8_t* data, size_t len) {
        if (_ws && _ws->connected()) {
            _ws->send(data, len);
        }
    });
}

// ========================================
// PUBLIC API - INITIALIZATION
// ========================================
/**
 * @brief Initializes POTA library with optional Wi-Fi management
 * 
 * This method can work in two modes:
 * 
 * Mode 1 - POTA manages Wi-Fi (ssid and password provided):
 * 1. Validates Wi-Fi credentials
 * 2. Resets Wi-Fi radio for clean state
 * 3. Connects to Wi-Fi network (30s timeout)
 * 4. Enables automatic Wi-Fi reconnection
 * 5. Sets up POTA configuration
 * 
 * Mode 2 - User manages Wi-Fi (ssid and password omitted):
 * 1. Assumes Wi-Fi is already connected
 * 2. Only sets up POTA configuration
 * 
 * @note Blocks until Wi-Fi connection succeeds or times out (when managing Wi-Fi)
 */
POTAError POTA::begin(const char* deviceType,
                      const char* firmwareVersion,
                      const char* authToken,
                      const char* serverSecret,
                      const char* ssid,
                      const char* password) 
{
    // If ssid is provided, manage Wi-Fi connection
    if (ssid != nullptr && strlen(ssid) > 0) {
        // Validate password when ssid is provided
        if (!password || strlen(password) == 0) 
            return POTAError::PARAMETER_INVALID_PASSWORD;
        
        // HOTSPOT COMPATIBILITY: Skip aggressive WiFi reset
        // Some mobile hotspots don't handle disconnect/WIFI_OFF well
        // Just set mode and connect directly
        Serial.println(F("[POTA] 📶 Preparing WiFi connection..."));
        
        #if defined(ESP32) || defined(ESP8266)
            WiFi.mode(WIFI_STA); // Station mode only
            delay(100);
        #endif
        
        // Initiate Wi-Fi connection
        Serial.print(F("[POTA] 📶 Connecting to Wi-Fi: "));
        Serial.print(ssid);
        WiFi.begin(ssid, password);
        unsigned long start = millis();
        
        // Wait for connection with 30-second timeout
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            if (millis() - start > 30000) return POTAError::WIFI_CONNECT_FAILED;
        }
        Serial.println("\n[POTA] ✅ Wi-Fi connected, IP: " + WiFi.localIP().toString());
        
        // Enable automatic reconnection after connection loss
        #if defined(ESP32) || defined(ESP8266)
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
        #endif
    }
    
    // Validate POTA parameters with size checks to prevent buffer overflows
    if (!deviceType || strlen(deviceType) == 0 || strlen(deviceType) >= sizeof(_deviceType)) 
        return POTAError::PARAMETER_INVALID_DEVICETYPE;
    
    if (!firmwareVersion || strlen(firmwareVersion) == 0 || strlen(firmwareVersion) >= sizeof(_firmwareVersion)) 
        return POTAError::PARAMETER_INVALID_FWVERSION;
    
    if (!authToken || strlen(authToken) == 0 || strlen(authToken) >= sizeof(_authToken)) 
        return POTAError::PARAMETER_INVALID_AUTHTOKEN;
    
    if (!serverSecret || strlen(serverSecret) == 0 || strlen(serverSecret) >= sizeof(_serverSecret)) 
        return POTAError::PARAMETER_INVALID_SECRET;

    // Safely copy strings with null-termination guarantees
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

/**
 * @brief Performs complete Wi-Fi radio reset
 * 
 * Three-stage reset process:
 * 1. Disconnect from current network
 * 2. Turn off Wi-Fi radio completely
 * 3. Re-enable in station mode
 * 
 * Delays ensure each stage completes before proceeding.
 */
void POTA::resetWiFi() {
    #if defined(ESP32) || defined(ESP8266)
        // ESP32/ESP8266: Full three-stage reset
        Serial.println(F("[POTA] 🔄 Resetting WiFi..."));
        
        WiFi.disconnect(true);  // Disconnect and erase credentials
        delay(500);             // Increased delay for hotspot compatibility
    
        WiFi.mode(WIFI_OFF);    // Power down radio
        delay(500);             // Increased delay for hotspot compatibility
    
        WiFi.mode(WIFI_STA);    // Re-enable in station mode
        delay(300);
        
    #elif defined(ARDUINO_OPTA)
        // Opta: NO-OP         
               
    #endif
}

/**
 * @brief Platform-specific safe system restart
 * 
 * Ensures WiFi module is properly shut down before rebooting to prevent
 * connection issues on next boot, particularly on Arduino Opta with hotspots.
 */
void POTA::restart() {
    
    #if defined(ESP32)
        Serial.println(F("[POTA] 🔄 ESP32 restart"));
        delay(100);
        ESP.restart();
        
    #elif defined(ESP8266)
        Serial.println(F("[POTA] 🔄 ESP8266 restart"));
        delay(100);
        ESP.restart();
        
    #elif defined(ARDUINO_OPTA)
        // Opta: CRITICAL - Must shutdown WiFi cleanly before reset
        // This prevents connection issues with hotspots on next boot
        Serial.println(F("[POTA] 🔄 Shutting down WiFi module..."));
        
        WiFi.disconnect();
        delay(500);
        
        WiFi.end();
        delay(500);
        
        Serial.println(F("[POTA] 🔄 Arduino OPTA restart"));
        delay(100);
        
        NVIC_SystemReset();
    #endif
    
    // Never reached, but prevents compiler warnings
    while(1);
}

/**
 * @brief Main loop function - handles all background POTA operations
 * 
 * This function should be called from Arduino loop(). It manages:
 * 
 * 1. WebSocket Auto-Reconnection:
 *    - Checks connection health
 *    - Attempts reconnection with 20s cooldown on failure
 *    - Prevents rapid reconnection attempts
 * 
 * 2. WebSocket Event Processing:
 *    - Processes incoming messages
 *    - Handles ping/pong heartbeats
 *    - Dispatches dashboard events
 * 
 * 3. Dashboard Synchronization:
 *    - Sends pending widget updates
 *    - Handles value quantization
 *    - Manages chart data transmission
 * 
 * @param enableAutoReconnect Enables WebSocket reconnection logic
 */
void POTA::loop(bool enableAutoReconnect) {
    POTAError err;
    static unsigned long nextReconnectAttempt = 0;
    const unsigned long reconnectInterval = 20000; // 20-second cooldown between attempts
    
    // WebSocket reconnection logic with exponential backoff
    if (enableAutoReconnect && (millis() >= nextReconnectAttempt) ) {
        err = ensureWebSocketConnection();
        if (err != POTAError::SUCCESS) {
            Serial.print(F("[POTA] ⚠️ Warning: Failure on websocket: "));
            Serial.println(errorToString(err));
            nextReconnectAttempt = millis() + reconnectInterval; // Schedule next attempt
        }
    // All platforms use NuSock
    } else if (_ws && _ws->connected()) {
        _ws->loop(); // Process WebSocket events
    }
    
    // Send pending dashboard updates to connected clients
    _dashboard->sendUpdates();
}

// ========================================
// DEVICE IDENTIFICATION
// ========================================
/**
 * @brief Retrieves platform-specific secure MAC address
 * 
 * Platform implementations:
 * - ESP32: Uses eFuse MAC (factory-programmed, read-only)
 * - ESP8266: Uses Wi-Fi interface MAC
 * - Arduino Opta: Uses board info structure MAC
 * 
 * @return MAC address formatted as "XX:XX:XX:XX:XX:XX" (uppercase hex)
 */
String POTA::getSecureMACAddress() {
#if defined(ESP32)
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac); // Read from eFuse (tamper-resistant)
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
    
#elif defined(ESP8266)
    return WiFi.macAddress(); // Returns formatted MAC string directly
    
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

// ========================================
// OTA UPDATE WORKFLOW
// ========================================
/**
 * @brief High-level OTA update function (check + perform)
 * 
 * This convenience method combines:
 * 1. checkOTAUpdate() - Query server for available updates
 * 2. performOTA() - Download and apply firmware
 * 
 * @return POTAError::SUCCESS (never returns on successful update - device reboots),
 *         POTAError::NO_UPDATE_AVAILABLE (already up-to-date),
 *         or specific error code
 */
POTAError POTA::checkAndPerformOTA() {
    char otaUrl[256]; // Buffer for firmware download URL
    POTAError err = checkOTAUpdate(otaUrl, sizeof(otaUrl));
    if (err != POTAError::SUCCESS) return err;

    return performOTA(otaUrl); // Reboots on success
}

// ========================================
// CRYPTOGRAPHIC HELPERS
// ========================================
/**
 * @brief Platform-agnostic HMAC-SHA256 calculation
 * 
 * Generates 64-character hexadecimal HMAC signature using:
 * - mbedTLS (ESP32, Arduino Opta)
 * - BearSSL (ESP8266)
 * 
 * HMAC process:
 * 1. Initialize HMAC context with secret key
 * 2. Update context with message data
 * 3. Finalize to produce 32-byte binary hash
 * 4. Convert binary to 64-character hex string
 * 
 * @param message Plaintext string to sign
 * @param secret Secret key for HMAC calculation
 * @param outToken Output buffer (must be >= 65 bytes for null terminator)
 * @param outTokenSize Size of output buffer
 * @return POTAError::SUCCESS or parameter validation error
 */
POTAError POTA::_calculateHMAC(const char* message, 
                               const char* secret, 
                               char* outToken, 
                               size_t outTokenSize)
{
    // Input validation
    if (!secret) return POTAError::PARAMETER_INVALID_SECRET;
    if (!outToken || outTokenSize < 65) return POTAError::PARAMETER_INVALID_OUTPUT;
    if (!message || strlen(message) == 0) return POTAError::TOKEN_GENERATION_FAILED;

    unsigned char hmac[32]; // SHA256 produces 32-byte output

    // Platform-specific HMAC calculation
    #if defined(ESP32) || defined(ARDUINO_OPTA)
        // mbedTLS implementation
        mbedtls_md_context_t ctx;
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, info, 1); // 1 = HMAC mode
        mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret, strlen(secret));
        mbedtls_md_hmac_update(&ctx, (const unsigned char*)message, strlen(message));
        mbedtls_md_hmac_finish(&ctx, hmac);
        mbedtls_md_free(&ctx);
        
    #elif defined(ESP8266)
        // BearSSL implementation
        br_hmac_key_context kc;
        br_hmac_context ctx;
        br_hmac_key_init(&kc, &br_sha256_vtable, secret, strlen(secret));
        br_hmac_init(&ctx, &kc, 32);  // 32 = SHA256 output size
        br_hmac_update(&ctx, message, strlen(message));
        br_hmac_out(&ctx, hmac);
    #endif

    // Convert binary HMAC to lowercase hexadecimal string
    static const char hexChars[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        outToken[i*2]       = hexChars[(hmac[i] >> 4) & 0x0F]; // High nibble
        outToken[i*2 + 1] = hexChars[hmac[i] & 0x0F];        // Low nibble
    }
    outToken[64] = '\0'; // Null terminator
    return POTAError::SUCCESS;
}

/**
 * @brief Generates HMAC token for OTA server response verification
 * 
 * Constructs signature message from OTA metadata fields:
 * "update:version:url:checksum:protocol_version:notes:timestamp"
 * 
 * The server must generate an identical signature using the same
 * secret key. If signatures match, we can trust the response hasn't
 * been tampered with (integrity) and came from our server (authenticity).
 * 
 * @param update Boolean indicating if update is available
 * @param version Firmware version string (e.g., "2.0.1")
 * @param url HTTPS download URL for firmware binary
 * @param checksum MD5/SHA256 checksum of firmware file
 * @param protocol_version POTA protocol version
 * @param notes Human-readable release notes
 * @param timestamp Server-provided Unix epoch timestamp
 * @param secret Shared secret key (must match server)
 * @param outToken Output buffer (>= 65 bytes)
 * @param outTokenSize Size of output buffer
 * @return POTAError::SUCCESS or generation error
 */
POTAError POTA::generateOtaServerToken(bool update,
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
    
    // Construct signature message with colon-separated fields
    // Empty fields are included as empty strings to maintain field order
    int n = snprintf(message, sizeof(message), "%s:%s:%s:%s:%s:%s:%s",
                     update ? "true" : "false",
                     version ? version : "",
                     url ? url : "",
                     checksum ? checksum : "",
                     protocol_version ? protocol_version : "",
                     notes ? notes : "",
                     timestamp ? timestamp : "");
                     
    // Check for buffer overflow or snprintf error
    if (n < 0 || n >= (int)sizeof(message)) return POTAError::TOKEN_GENERATION_FAILED;

    // Delegate to platform-agnostic HMAC calculation
    return _calculateHMAC(message, secret, outToken, outTokenSize);
}

/**
 * @brief Generates HMAC token for WebSocket connection verification
 * 
 * Constructs signature message from WebSocket credentials:
 * "success:ws_host:ws_path:device_id:timestamp"
 * 
 * The path includes the JWT authentication token. This prevents
 * unauthorized WebSocket connections and ensures the credentials
 * came from the legitimate server.
 * 
 * @param success Literal "True" or "False" from server response
 * @param ws_host WebSocket hostname (e.g., "ws.pleasedontcode.com")
 * @param ws_path WebSocket path including JWT (e.g., "/ws/device/?jwt=...")
 * @param device_id Device MAC address
 * @param timestamp Server Unix epoch timestamp
 * @param secret Shared secret key (must match server)
 * @param outToken Output buffer (>= 65 bytes)
 * @param outTokenSize Size of output buffer
 * @return POTAError::SUCCESS or generation error
 */
POTAError POTA::generateWsServerToken(const char* success,
                                      const char* ws_host,
                                      const char* ws_path,
                                      const char* device_id,
                                      const char* timestamp,
                                      const char* secret,
                                      char* outToken, size_t outTokenSize)
{
    if (!secret) return POTAError::PARAMETER_INVALID_SECRET;
    if (!outToken || outTokenSize < 65) return POTAError::PARAMETER_INVALID_OUTPUT;

    char message[512];
    
    // Construct signature message with host and path as separate fields
    int n = snprintf(message, sizeof(message), "%s:%s:%s:%s:%s",
                     success ? success : "false", 
                     ws_host ? ws_host : "",
                     ws_path ? ws_path : "",
                     device_id ? device_id : "",
                     timestamp ? timestamp : "");

    if (n < 0 || n >= (int)sizeof(message)) return POTAError::TOKEN_GENERATION_FAILED;

    return _calculateHMAC(message, secret, outToken, outTokenSize);
}

// ========================================
// WEBSOCKET CONNECTION MANAGEMENT
// ========================================
/**
 * @brief Global instance pointer for WebSocket callback bridge
 *
 * NuSock library requires C-style function pointers for callbacks.
 * We use this global to bridge to C++ member function.
 */
static POTA* _potaInstance = nullptr;

/**
 * @brief C-style bridge function for NuSock events
 *
 * Forwards events to the actual POTA instance's member function.
 *
 * @param client NuClient pointer (unused)
 * @param event Event type (connected, disconnected, message, etc.)
 * @param payload Event data payload
 * @param len Payload length in bytes
 */
void nuSockBridge(NuClient* client, NuClientEvent event, const uint8_t* payload, size_t len) {
    if (_potaInstance != nullptr) {
        _potaInstance->remoteWebSocketEvent((uint8_t)event, (uint8_t*)payload, len);
    }
}

#if defined(ESP8266)
/**
 * @brief ESP8266: Configure SSL client with secure certificate validation
 * 
 * Always configures with certificate validation.
 * Individual connections will retry with setInsecure() if this fails.
 * 
 * @param client WiFiClientSecure client to configure
 */
void configureESP8266SSLClient(WiFiClientSecure& client) {
    client.setBufferSizes(1024, 1024); // Reduce RAM usage (1KB instead of 16KB)
    // Always start with secure mode - connection functions handle fallback
    client.setTrustAnchors(new BearSSL::X509List(root_ca));
}

/**
 * @brief ESP8266: Reset WebSocket to try secure mode again
 * 
 * Called when WebSocket disconnects to ensure next connection
 * attempt starts with secure certificate validation.
 */
static bool _wsUseInsecureMode = false;
void resetWebSocketSecurityMode() {
    _wsUseInsecureMode = false;
}
#endif

/**
 * @brief Ensures WebSocket connection is active (lazy initialization)
 * 
 * State machine:
 * 1. If connected: return immediately
 * 2. If not created: create WebSocket client object (one-time)
 * 3. If not connecting: fetch credentials and initiate connection
 * 
 * Non-blocking design: Connection is initiated but not waited for.
 * Actual connection success/failure is handled via event callbacks.
 * 
 * ESP8266 secure fallback: If connection fails with certificate,
 * will automatically retry with setInsecure() on next call.
 * 
 * @return POTAError::SUCCESS if connection process initiated successfully
 * 
 * @return POTAError::SUCCESS if connected or connection initiated,
 *         error code if credential fetch failed
 */
POTAError POTA::ensureWebSocketConnection() {
    #if defined(ESP8266)
        // Static variables for timeout tracking
        static unsigned long wsConnectStart = 0;
        const unsigned long WS_CONNECT_TIMEOUT = 10000; // 10 seconds
    #endif

    if(_ws) _ws->loop(); // Process pending events
    if (_wsConnected) return POTAError::SUCCESS; // Already connected
    
    #if defined(ESP8266)
        // Check if secure connection attempt timed out
        if (_wsConnecting && !_wsConnected && !_wsUseInsecureMode) {
            if (millis() - wsConnectStart > WS_CONNECT_TIMEOUT) {
                Serial.println(F("[POTA] ⚠️  Secure WebSocket timeout, switching to insecure mode..."));
                _wsUseInsecureMode = true;
                _wsConnecting = false; // Force reconnection with insecure
                
                // Delete and recreate WebSocket to reset state
                delete _ws;
                _ws = nullptr;
            }
        }
    #endif
    
    // ONE-TIME: Create WebSocket client object
    if (!_ws) {
        Serial.println(F("[POTA] ⚙️ Creating WebSocketsClient"));
        _ws = new WS_CLIENT_TYPE();
        
        // All platforms: NuSock requires C-style callback
        _potaInstance = this;
        _ws->onEvent(nuSockBridge);
    }

    // PREPARE CONNECTION: Fetch WebSocket URL and JWT from API (blocking HTTPS call)
    if(!_wsConnecting) {
        char wsHost[64];
        char wsPath[512];
    
        Serial.println(F("[POTA] 🌐 Preparing WebSocket URL..."));
        POTAError err = getWebSocketUrl(wsHost, sizeof(wsHost), wsPath, sizeof(wsPath));
        if (err != POTAError::SUCCESS) {
            Serial.print(F("[POTA] ❌ getWebSocketUrl failed: "));
            Serial.println(errorToString(err));
            return err;
        }
        
        // INITIATE SSL CONNECTION
        Serial.println(F("[POTA] 🔒 Opening WS SSL..."));
        
        #if defined(ESP8266)
            // ESP8266: Try secure first, fallback to insecure on timeout
            static WiFiClientSecure wsClient;
            static WiFiClientSecure* client = &wsClient;
            
            wsClient.setBufferSizes(1024, 1024);
            
            // Use secure or insecure based on previous attempt
            if (_wsUseInsecureMode) {
                wsClient.setInsecure();
                Serial.println(F("[POTA] ⚠️  Using insecure mode for WebSocket"));
            } else {
                static BearSSL::X509List cert(root_ca);
                wsClient.setTrustAnchors(&cert);
                wsConnectStart = millis(); // Start timeout timer
            }
            
            _ws->begin(client, wsHost, 443, wsPath);
            _ws->connect(); // Non-blocking
            
        #elif defined(ARDUINO_OPTA)
            // Opta: Use secure certificate validation
            static WiFiSSLClient wsClient;
            static WiFiSSLClient* client = &wsClient;
            
            wsClient.appendCustomCACert(root_ca);
            
            _ws->begin(client, wsHost, 443, wsPath);
            _ws->connect(); // Non-blocking
            
        #elif defined(ESP32)
            // ESP32: Use static WiFiClientSecure to avoid heap fragmentation
            // (reuses same SSL context instead of allocating a new one each time)
            static WiFiClientSecure wsClient;
            static WiFiClientSecure* client = &wsClient;

            wsClient.setCACert(root_ca);

            _ws->begin(client, wsHost, 443, wsPath);
            _ws->connect();
        #endif
        
        _wsConnecting = true; // Mark connection in progress
        
    }
    
    return POTAError::SUCCESS;
}

/**
 * @brief WebSocket event handler (called by WebSocket library)
 * 
 * Handles all WebSocket events:
 * - CONNECTED: Updates connection status, notifies dashboard
 * - DISCONNECTED: Clears connection status
 * - BIN: Processes MessagePack messages (system commands + dashboard data)
 * - ERROR: Logs error and clears connection
 * - PING/PONG: Heartbeat mechanism (handled internally)
 * 
 * System commands are intercepted here:
 * - "ota_available": Triggers user callback with new version info
 * 
 * Dashboard data is forwarded to dashboard.decodeEventData().
 * 
 * @param type Event type (platform-agnostic uint8_t)
 * @param payload Event data pointer
 * @param length Payload size in bytes
 */
void POTA::remoteWebSocketEvent(uint8_t type, uint8_t * payload, size_t length) {
    
    // All platforms use NuSock - convert NuClientEvent to WStype_t for common handling
    WStype_t wsType;
    switch(type) {
        case CLIENT_EVENT_CONNECTED:       wsType = WStype_CONNECTED; break;
        case CLIENT_EVENT_DISCONNECTED:    wsType = WStype_DISCONNECTED; break;
        case CLIENT_EVENT_MESSAGE_TEXT:     wsType = WStype_TEXT; break;
        case CLIENT_EVENT_MESSAGE_BINARY:  wsType = WStype_BIN; break;
        case CLIENT_EVENT_ERROR:           wsType = WStype_ERROR; break;
        default: return; // Ignore minor events (handshake, fragments, etc.)
    }
    
    // Process event based on type
    switch(wsType) {
        case WStype_CONNECTED:
            _wsConnecting = false;
            _wsConnected = true;
            _dashboard->setConnectionStatus(true);
            Serial.println(F("[POTA] WS Connected!"));
            break;

        case WStype_DISCONNECTED:
            Serial.println(F("[POTA] WS Disconnected"));
            _wsConnecting = false;
            _wsConnected = false;
            _dashboard->setConnectionStatus(false);
            
            #if defined(ESP8266)
                // Reset to secure mode for next connection attempt
                resetWebSocketSecurityMode();
                Serial.println(F("[POTA] 🔄 Next connection will try secure mode first"));
            #endif
            break;
            
        case WStype_ERROR:
            _wsConnected = false;
            _wsConnecting = false;
            _dashboard->setConnectionStatus(false);
            Serial.println(F("[POTA] WS error"));
            
            #if defined(ESP8266)
                // Reset to secure mode for next connection attempt
                resetWebSocketSecurityMode();
            #endif
            break;

        case WStype_TEXT:
            // Text messages not currently used (dashboard uses binary MessagePack)
            break;
            
        case WStype_BIN:
            {
                // Decode MessagePack binary message
                #if ARDUINOJSON_VERSION_MAJOR == 6
                    StaticJsonDocument<512> doc;
                #else
                    JsonDocument doc;
                #endif
    
                DeserializationError error = deserializeMsgPack(doc, payload, length);
    
                if (!error) {
                    const char* command = doc["command"];
                    
                    // --- SYSTEM COMMAND INTERCEPTION ---
                    // "ota_available" is a POTA system notification (not dashboard data)
                    if (command && strcmp(command, "ota_available") == 0) {
                        const char* newVersion = doc["version"] | "unknown";
                        Serial.print(F("[POTA] 🚀  System Notification: New firmware: "));
                        Serial.print(newVersion);
                        Serial.println(F(" is available!"));
                                                
                        // Trigger user-defined callback if registered
                        if (_otaAvailableCallback != nullptr) {
                            _otaAvailableCallback(newVersion);
                        }
                        
                        break; // Don't forward system commands to dashboard
                    }
                } else {
                    Serial.print(F("[POTA] ❌ MsgPack deserialization failed: "));
                    Serial.println(error.c_str());
                }
    
                // --- DASHBOARD DATA FORWARDING ---
                // If not a system command, forward to dashboard for widget processing
                if (_dashboard) {
                    _dashboard->decodeEventData(payload, length);
                }
            }
            break;
                        
        case WStype_PONG:
            // Heartbeat response (handled automatically)
            break;
            
        case WStype_PING:
            // Heartbeat request (library auto-responds)
            break;
            
        default:
            break;
    }
}

// ========================================
// OTA UPDATE CHECK
// ========================================
/**
 * @brief Queries POTA API server for firmware updates
 * 
 * Process:
 * 1. Establishes HTTPS connection to API server
 * 2. Sends POST request with device info (MAC, type, version, token)
 * 3. Reads and parses JSON response
 * 4. Verifies HMAC signature to ensure response authenticity
 * 5. Returns download URL if update available
 * 
 * Security verification:
 * - Server response includes HMAC token generated from response fields
 * - Client recalculates expected token using shared secret
 * - Comparison failure indicates tampering or secret mismatch
 * 
 * @param outOTAUrl Buffer to store firmware download URL
 * @param outOTAUrlSize Size of output buffer
 * @return POTAError::SUCCESS if update available,
 *         POTAError::NO_UPDATE_AVAILABLE if up-to-date,
 *         or specific error code
 */
POTAError POTA::checkOTAUpdate(char* outOTAUrl, size_t outOTAUrlSize) {
    // Input validation
    if (!outOTAUrl || outOTAUrlSize == 0) return POTAError::PARAMETER_INVALID_OUTPUT;
    outOTAUrl[0] = '\0'; // Initialize to empty string
    
    // Create platform-specific HTTPS client
    #if defined(ESP32)
        WiFiClientSecure client;
        client.setCACert(root_ca);
    #elif defined(ESP8266)
        WiFiClientSecure client;
        configureESP8266SSLClient(client);
    #elif defined(ARDUINO_OPTA)
        WiFiSSLClient client;
        client.appendCustomCACert(root_ca);
    #endif

    // Establish HTTPS connection to API server
    if (!client.connect(API_HOST, 443)) {
        #if defined(ESP8266)
            // ESP8266: Retry with insecure mode (hotspot compatibility)
            Serial.println(F("[POTA] ⚠️  Secure connection failed, retrying without certificate validation..."));
            
            client.stop();
            delay(100);
            
            // Reconfigure to insecure mode
            client.setInsecure();
            
            // Retry connection
            if (!client.connect(API_HOST, 443)) {
                client.stop();
                Serial.println(F("[POTA] ❌ Connection failed in both secure and insecure modes"));
                return POTAError::CONNECTION_FAILED;
            }
            
            Serial.println(F("[POTA] ⚠️  Connected without certificate validation (insecure mode)"));
        #else
            client.stop();
            return POTAError::CONNECTION_FAILED;
        #endif
    }
    Serial.println(F("[POTA] 🔗 Connected to server"));

    // Single buffer reused for both request and response (memory optimization)
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

    // Validate buffer usage
    if (bodyLen < 0 || bodyLen >= (int)sizeof(buffer)) {
        Serial.println(F("[POTA] ❌ BUFFER_OVERFLOW_REQUEST while building JSON request"));
        client.stop();
        return POTAError::BUFFER_OVERFLOW_REQUEST;
    }

    // --- Send HTTP POST request ---
    client.println("POST " CHECK_UPDATE_API " HTTP/1.1");
    client.println("Host: " API_HOST);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(bodyLen);
    client.println("Connection: close");
    client.println(); // End of headers
    client.println(buffer); // Send JSON body

    // Wait for server response
    while (client.connected() && !client.available()) delay(10);

    // --- Skip HTTP headers (read until empty line) ---
    while (client.available()) {
        char line[128];
        size_t len = client.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        if (strcmp(line, "\r") == 0) break; // Empty line marks end of headers
    }

    // --- Read HTTP response body into same buffer ---
    size_t len = client.readBytes(buffer, sizeof(buffer) - 1);
    
    // Check for buffer overflow
    if (len >= sizeof(buffer) - 1) {
        Serial.println(F("[POTA] ❌ BUFFER_OVERFLOW_RESPONSE while reading server response"));
        client.stop();
        return POTAError::BUFFER_OVERFLOW_RESPONSE;
    }

    buffer[len] = '\0'; // Null terminate
    
    client.stop();

    // --- Parse JSON response ---
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, buffer);
    if (error) {
        Serial.print(F("[POTA] ❌ JSON parse failed: "));
        Serial.println(error.c_str());
        return POTAError::JSON_PARSE_FAILED;
    }

    // Extract response fields with defaults
    bool update = doc["update"] | false;
    const char* url = doc["url"] | "";
    const char* version = doc["version"] | "";
    const char* checksum = doc["checksum"] | "";
    const char* protocol_version = doc["protocol_version"] | "";
    const char* notes = doc["notes"] | "";
    const char* server_token = doc["server_token"] | "";
    const char* errorMsg = doc["error"] | "";
    long timestampValue = doc["timestamp"] | 0;
    
    // Convert timestamp to string for HMAC calculation
    char timestampStr[32];
    snprintf(timestampStr, sizeof(timestampStr), "%ld", timestampValue);

    // Check for server error message
    if (strlen(errorMsg) > 0) {
        Serial.print(F("[POTA] ❌ Server error message: "));
        Serial.println(errorMsg);
        return POTAError::SERVER_ERROR_4XX;
    }

    // --- Verify server HMAC token for security ---
    char expectedToken[65];
    POTAError err = generateOtaServerToken(update, version, url, checksum,
                                        protocol_version, notes, timestampStr,
                                        _serverSecret, expectedToken, sizeof(expectedToken));
    if (err != POTAError::SUCCESS) return err;

    // Compare calculated token with server-provided token
    if (strcmp(expectedToken, server_token) != 0) return POTAError::TOKEN_MISMATCH;

    // --- Process update availability ---
    if (update && strncmp(url, "https://" API_HOST, strlen("https://" API_HOST)) == 0) {
        Serial.print(F("[POTA] ⬆️ New firmware version available: "));
        Serial.println(version);
        Serial.print(F("[POTA] 📝 Notes: "));
        Serial.println(notes);
        
        // Copy download URL to output buffer
        strncpy(outOTAUrl, url, outOTAUrlSize - 1);
        outOTAUrl[outOTAUrlSize - 1] = '\0';
        return POTAError::SUCCESS;
    }

    // No update available
    return POTAError::NO_UPDATE_AVAILABLE;
}

/**
 * @brief Fetches WebSocket connection credentials from API
 * 
 * Similar to checkOTAUpdate() but for WebSocket setup:
 * 1. Connects to API server via HTTPS
 * 2. Sends device authentication
 * 3. Receives WebSocket host, path (with JWT), and HMAC token
 * 4. Verifies token authenticity
 * 5. Returns verified credentials
 * 
 * The path includes a JWT token for WebSocket authentication.
 * 
 * @param outWSHost Buffer for WebSocket hostname
 * @param outWSHostSize Size of host buffer
 * @param outWSPath Buffer for WebSocket path (includes JWT)
 * @param outWSPathSize Size of path buffer
 * @return POTAError::SUCCESS if credentials valid, error otherwise
 */
 
POTAError POTA::getWebSocketUrl(char* outWSHost, size_t outWSHostSize, char* outWSPath, size_t outWSPathSize) {
    if (!outWSHost || outWSHostSize == 0 || !outWSPath || outWSPathSize == 0) {
        return POTAError::PARAMETER_INVALID_OUTPUT;
    }
    
    // Initialize output buffers
    outWSHost[0] = '\0';
    outWSPath[0] = '\0';
    
    // Create platform-specific HTTPS client
    #if defined(ESP32)
        WiFiClientSecure client;
        client.setCACert(root_ca);
    #elif defined(ESP8266)
        WiFiClientSecure client;
        configureESP8266SSLClient(client);
    #elif defined(ARDUINO_OPTA)
        WiFiSSLClient client;
        client.appendCustomCACert(root_ca);
    #endif

    if (!client.connect(API_HOST, 443)) {
        #if defined(ESP8266)
            // ESP8266: Retry with insecure mode (hotspot compatibility)
            Serial.println(F("[POTA] ⚠️  Secure connection failed, retrying without certificate validation..."));
            
            client.stop();
            delay(100);
            client.setInsecure();
            
            if (!client.connect(API_HOST, 443)) {
                client.stop();
                Serial.println(F("[POTA] ❌ Connection failed in both secure and insecure modes"));
                return POTAError::CONNECTION_FAILED;
            }
            
            Serial.println(F("[POTA] ⚠️  Connected without certificate validation (insecure mode)"));
        #else
            client.stop();
            return POTAError::CONNECTION_FAILED;
        #endif
    }
    Serial.println(F("[POTA] 🔗 Connected to server for WS token"));

    const size_t BUF_SIZE = 1024;
    char* buffer = (char*)malloc(BUF_SIZE);
    if (!buffer) {
        client.stop();
        return POTAError::SERVER_ERROR_4XX; 
    }

    // --- Build JSON request ---
    int bodyLen = snprintf(buffer, BUF_SIZE,
                 "{"
                 "\"device_id\":\"%s\","
                 "\"auth_token\":\"%s\""
                 "}",
                 getSecureMACAddress().c_str(),
                 _authToken);

    if (bodyLen < 0 || (size_t)bodyLen >= BUF_SIZE) {
        Serial.println("[POTA] ❌ BUFFER_OVERFLOW_REQUEST while building WS JSON request");
        client.stop();
        if (buffer) free(buffer); 
        return POTAError::BUFFER_OVERFLOW_REQUEST;
    }

    // --- Send HTTP POST request ---
    client.println("POST " GET_WS_TOKEN_API " HTTP/1.1");
    client.println("Host: " API_HOST);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(bodyLen);
    client.println("Connection: close");
    client.println();
    client.println(buffer);

    // --- Read response ---
    while (client.connected() && !client.available()) delay(10);
    while (client.available()) {
        char line[128];
        size_t lenLine = client.readBytesUntil('\n', line, sizeof(line) - 1);
        line[lenLine] = '\0';
        if (strcmp(line, "\r") == 0) break;
    }

    size_t len = client.readBytes(buffer, BUF_SIZE - 1);
    
    if (len >= BUF_SIZE - 1) {
        Serial.println(F("[POTA] ❌ BUFFER_OVERFLOW_RESPONSE while reading WS response"));
        client.stop();
        if (buffer) free(buffer);
        return POTAError::BUFFER_OVERFLOW_RESPONSE;
    }
    buffer[len] = '\0';
    client.stop();

    // --- Parse JSON response ---
    DynamicJsonDocument doc(BUF_SIZE);
    DeserializationError error = deserializeJson(doc, buffer);
    if (error) {
        Serial.print(F("[POTA] ❌ WS JSON parse failed: "));
        Serial.println(error.c_str());
        if (buffer) free(buffer);
        return POTAError::JSON_PARSE_FAILED;
    }

    // Extract WebSocket credentials
    bool successValue = doc["success"] | false;
    const char* ws_host = doc["websocket_host"] | "";
    const char* ws_path = doc["websocket_path"] | "";
    const char* device_id_resp = doc["device_id"] | "";
    const char* server_token = doc["server_token"] | "";
    const char* errorMsg = doc["error"] | "";
    long timestampValue = doc["timestamp"] | 0;

    char timestampStr[32];
    snprintf(timestampStr, sizeof(timestampStr), "%ld", timestampValue);
    
    if (strlen(errorMsg) > 0) {
        Serial.print(F("[POTA] ❌ Server error message (WS): "));
        Serial.println(errorMsg);
        if (buffer) free(buffer);
        return POTAError::SERVER_ERROR_4XX;
    }
    
    if (!successValue) {
        Serial.println(F("[POTA] ❌ Server did not report success in payload (WS)"));
        if (buffer) free(buffer);
        return POTAError::SERVER_ERROR_4XX;
    }

    // --- Verify HMAC token ---
    char expectedToken[65];
    char successStr[] = "True"; 
    
    POTAError err = generateWsServerToken(successStr, 
                                          ws_host,
                                          ws_path, 
                                          device_id_resp, 
                                          timestampStr,
                                          _serverSecret, 
                                          expectedToken, 
                                          sizeof(expectedToken));
    if (err != POTAError::SUCCESS) {
        if (buffer) free(buffer);
        return err;
    }

    if (strcmp(expectedToken, server_token) != 0) {
        Serial.println(F("[POTA] ❌ WS Server token mismatch. Security breach or secret mismatch."));
        if (buffer) free(buffer);
        return POTAError::TOKEN_MISMATCH;
    }

    // --- Success: Copy credentials to output buffers ---
    if (strlen(ws_host) > 0 && strlen(ws_path) > 0) {
        Serial.println(F("[POTA] ✅ WS Token, Host, and Path successfully verified."));

        if (strlen(ws_host) >= outWSHostSize) {
            Serial.println(F("[POTA] ❌ Host Buffer Overflow."));
            if (buffer) free(buffer);
            return POTAError::BUFFER_OVERFLOW_REQUEST;
        }
        memset(outWSHost, 0, outWSHostSize);
        strncpy(outWSHost, ws_host, outWSHostSize - 1);
        outWSHost[outWSHostSize - 1] = '\0';

        if (strlen(ws_path) >= outWSPathSize) {
            Serial.println(F("[POTA] ❌ Path Buffer Overflow."));
            if (buffer) free(buffer);
            return POTAError::BUFFER_OVERFLOW_REQUEST;
        }
        memset(outWSPath, 0, outWSPathSize);
        strncpy(outWSPath, ws_path, outWSPathSize - 1);
        outWSPath[outWSPathSize - 1] = '\0';
        
        if (buffer) free(buffer);
        return POTAError::SUCCESS;
    }
    
    if (buffer) free(buffer);
    return POTAError::SERVER_ERROR_4XX;
}

/**
 * @brief Performs platform-specific OTA firmware update
 * 
 * Platform implementations:
 * 
 * ESP32:
 * - Uses esp_https_ota library
 * - Automatically verifies certificate
 * - Downloads and flashes firmware
 * - Reboots on success
 * 
 * ESP8266:
 * - Uses ESP8266httpUpdate library
 * - Similar process to ESP32
 * 
 * Arduino Opta:
 * - Uses Arduino_Portenta_OTA library
 * - Three-stage process: download → decompress → apply
 * - Requires Wi-Fi firmware to be pre-installed
 * 
 * @warning This function reboots the device on successful update!
 * 
 * @param OTA_file_url HTTPS URL of firmware binary
 * @return POTAError::SUCCESS (never returns), or error code
 */
POTAError POTA::performOTA(const char* OTA_file_url) {
    if (!OTA_file_url || strlen(OTA_file_url) == 0) 
        return POTAError::PARAMETER_INVALID_OTA_URL;

#if defined(ESP32)
    // ESP32 OTA implementation
    Serial.println(F("[POTA] 🔍 Checking for OTA update..."));
    esp_http_client_config_t http_config = {
        .url = OTA_file_url,
        .cert_pem = root_ca,     // Certificate for HTTPS
        .timeout_ms = 10000,     // 10-second timeout
    };
    
    // ESP-IDF version detection for API compatibility
    #if ESP_IDF_VERSION_MAJOR < 5
        // ESP-IDF v4.x → Arduino ESP32 core 2.x
        esp_err_t ret = esp_https_ota(&http_config);
    #else
        // ESP-IDF v5.x → Arduino ESP32 core 3.x
        esp_https_ota_config_t ota_config = {
            .http_config = &http_config,
        };
        esp_err_t ret = esp_https_ota(&ota_config);
    #endif
    
    if (ret == ESP_OK) {
        Serial.println(F("[POTA] ✅ OTA update completed. Restarting..."));
        esp_restart(); // Never returns
        return POTAError::SUCCESS;
    } else {
        Serial.print(F("[POTA] ❌ OTA failed. Error: "));
        Serial.println(esp_err_to_name(ret));
        return POTAError::OTA_FAILED;
    }
    
#elif defined(ESP8266)
    // ESP8266 OTA implementation with adaptive SSL
    Serial.println(F("[POTA] 🔍 Checking for OTA update..."));
    
    // Try with secure certificate first
    WiFiClientSecure client;
    configureESP8266SSLClient(client);
    
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, String(OTA_file_url));
    
    if (ret == HTTP_UPDATE_FAILED) {
        // Retry with insecure mode (hotspot compatibility)
        Serial.println(F("[POTA] ⚠️  Secure OTA failed, retrying without certificate validation..."));
        
        client.setInsecure();
        ret = ESPhttpUpdate.update(client, String(OTA_file_url));
        
        if (ret == HTTP_UPDATE_FAILED) {
            Serial.printf("[POTA] ❌ OTA failed in both modes. Error (%d): %s\n", 
                          ESPhttpUpdate.getLastError(), 
                          ESPhttpUpdate.getLastErrorString().c_str());
            return POTAError::OTA_FAILED;
        }
    }
    
    Serial.println(F("[POTA] ✅ OTA update completed. Restarting..."));
    return POTAError::SUCCESS; // Automatic restart by library

#elif defined(ARDUINO_OPTA)
    // Arduino Opta OTA implementation (three-stage process)
    Serial.println(F("[POTA] 🔍 Checking for OTA update..."));

    // Initialize OTA with QSPI flash storage
    Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);
    if (!ota.isOtaCapable()) return POTAError::OTA_NOT_CAPABLE;

    Arduino_Portenta_OTA::Error err;
    if ((err = ota.begin()) != Arduino_Portenta_OTA::Error::None) 
        return POTAError::OTA_BEGIN_FAILED;

    // Stage 1: Download firmware from server
    Serial.println(F("[POTA] ⬇️ Starting OTA firmware download..."));
    int downloaded = ota.download(OTA_file_url, true);
    Serial.print(F("[POTA] ⬇️ Download result: "));
    Serial.println(downloaded);
    if (downloaded == -3011) return POTAError::OTA_WIFI_FW_MISSING;
    if (downloaded <= 0) return POTAError::OTA_DOWNLOAD_FAILED;
    Serial.println(F("[POTA] ✅ OTA firmware downloaded successfully."));

    // Stage 2: Decompress firmware (Opta uses compressed .ota files)
    Serial.println(F("[POTA] 🗜️ Decompressing OTA firmware..."));
    int decompressed = ota.decompress();
    Serial.print(F("[POTA] 🗜️ Decompression result: "));
    Serial.println(decompressed);
    if (decompressed <= 0) return POTAError::OTA_DECOMPRESSION_FAILED;
    Serial.println(F("[POTA] ✅ OTA firmware decompressed successfully."));

    // Stage 3: Apply firmware update
    Serial.println(F("[POTA] ⚡ Applying OTA update..."));
    if ((err = ota.update()) != Arduino_Portenta_OTA::Error::None) 
        return POTAError::OTA_APPLY_FAILED;

    Serial.println(F("[POTA] ✅ OTA update completed. Restarting..."));
    delay(1000);
    ota.reset(); // Reboot device
    return POTAError::SUCCESS;

#endif
}

/**
 * @brief Converts POTAError enum to human-readable string
 * 
 * Useful for debugging and user-friendly error messages.
 * 
 * @param err POTAError value to convert
 * @return Descriptive const char* string
 */
const char* POTA::errorToString(POTAError err) {
    switch (err) {
        case POTAError::SUCCESS: return PSTR("SUCCESS");
        case POTAError::PARAMETER_INVALID_SSID: return PSTR("Invalid SSID parameter");
        case POTAError::PARAMETER_INVALID_PASSWORD: return PSTR("Invalid Wi-Fi password parameter");
        case POTAError::PARAMETER_INVALID_DEVICETYPE: return PSTR("Invalid device type parameter");
        case POTAError::PARAMETER_INVALID_FWVERSION: return PSTR("Invalid firmware version parameter");
        case POTAError::PARAMETER_INVALID_AUTHTOKEN: return PSTR("Invalid authentication token parameter");
        case POTAError::PARAMETER_INVALID_SECRET: return PSTR("Invalid secret key parameter");
        case POTAError::PARAMETER_INVALID_OUTPUT: return PSTR("Output buffer is null or too small");
        case POTAError::PARAMETER_INVALID_OTA_URL: return PSTR("Invalid OTA URL parameter");
        case POTAError::WIFI_CONNECT_FAILED: return PSTR("Failed to connect to Wi-Fi");
        case POTAError::CLIENT_NOT_INITIALIZED: return PSTR("Wi-Fi client not initialized");
        case POTAError::CONNECTION_FAILED: return PSTR("Could not connect to server");
        case POTAError::JSON_PARSE_FAILED: return PSTR("Failed to parse JSON response");
        case POTAError::TOKEN_GENERATION_FAILED: return PSTR("Failed to generate server token");
        case POTAError::TOKEN_MISMATCH: return PSTR("Server token did not match expected");
        case POTAError::NO_UPDATE_AVAILABLE: return PSTR("No OTA update available");
        case POTAError::OTA_FAILED: return PSTR("OTA process failed (generic)");
        case POTAError::OTA_DOWNLOAD_FAILED: return PSTR("OTA firmware download failed");
        case POTAError::OTA_DECOMPRESSION_FAILED: return PSTR("OTA firmware decompression failed");
        case POTAError::OTA_APPLY_FAILED: return PSTR("OTA firmware application failed");
        case POTAError::OTA_NOT_CAPABLE: return PSTR("Portenta bootloader too old or not capable");
        case POTAError::OTA_BEGIN_FAILED: return PSTR("OTA initialization failed");
        case POTAError::PLATFORM_NOT_SUPPORTED: return PSTR("Board platform not supported");
        case POTAError::BUFFER_OVERFLOW_REQUEST: return PSTR("Buffer overflow while building JSON request");
        case POTAError::BUFFER_OVERFLOW_RESPONSE: return PSTR("Buffer overflow while reading server response");
        case POTAError::OTA_WIFI_FW_MISSING: return PSTR("Wi-Fi firmware not installed. Please run WifiFirmwareUpdater.ino / QSPIFormat.ino at least once before performing OTA.");
        case POTAError::SERVER_ERROR_4XX: return PSTR("Server returned a 4xx error");
        default: return PSTR("Undefined error");
    }
}