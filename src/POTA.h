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
      - Initialize OTA client with optional Wi-Fi management
      - Perform OTA update checks and apply firmware updates
      - Handle errors via POTAError codes and helper functions
      - Manage real-time dashboard widgets and WebSocket communication

  Usage:
    Include this header in your Arduino sketch:
      #include <POTA.h>

  Compatible boards:
    - ESP32 (using esp_https_ota)
    - ESP8266 (using ESPhttpUpdate)
    - Arduino Opta WiFi (using Arduino_Portenta_OTA)
*/

#pragma once

#include "potaDashboard.h"

/**
 * @enum POTAError
 * @brief Comprehensive error codes for all POTA library operations
 * 
 * All POTA functions return POTAError values instead of throwing exceptions.
 * Use errorToString() to get human-readable descriptions.
 * 
 * Error categories:
 * - PARAMETER_* : Invalid input parameters
 * - WIFI_* : Wi-Fi connectivity issues
 * - CLIENT_* : HTTP/HTTPS client problems
 * - JSON_* : Data parsing failures
 * - TOKEN_* : Security/authentication failures
 * - OTA_* : Firmware update process errors
 * - BUFFER_* : Memory/buffer overflow issues
 * - SERVER_* : API server errors
 */
enum class POTAError {
    SUCCESS = 0,                    ///< Operation completed successfully
    PARAMETER_INVALID_SSID,         ///< SSID parameter is null or empty
    PARAMETER_INVALID_PASSWORD,     ///< Password parameter is null or empty
    PARAMETER_INVALID_DEVICETYPE,   ///< Device type parameter is invalid
    PARAMETER_INVALID_FWVERSION,    ///< Firmware version parameter is invalid
    PARAMETER_INVALID_AUTHTOKEN,    ///< Authentication token parameter is invalid
    PARAMETER_INVALID_SECRET,       ///< Secret key parameter is invalid
    PARAMETER_INVALID_OUTPUT,       ///< Output buffer is null or too small
    PARAMETER_INVALID_OTA_URL,      ///< OTA URL parameter is invalid
    WIFI_CONNECT_FAILED,            ///< Failed to connect to Wi-Fi within timeout
    CLIENT_NOT_INITIALIZED,         ///< Wi-Fi client was not initialized (call begin first)
    CONNECTION_FAILED,              ///< Could not establish HTTPS connection to server
    JSON_PARSE_FAILED,              ///< Failed to parse JSON response from server
    TOKEN_GENERATION_FAILED,        ///< Failed to generate HMAC-SHA256 token
    TOKEN_MISMATCH,                 ///< Server token verification failed (security breach)
    NO_UPDATE_AVAILABLE,            ///< No OTA update available (already up-to-date)
    OTA_FAILED,                     ///< OTA process failed (generic/unknown error)
    OTA_DOWNLOAD_FAILED,            ///< OTA firmware download failed
    OTA_DECOMPRESSION_FAILED,       ///< OTA firmware decompression failed (Portenta)
    OTA_APPLY_FAILED,               ///< OTA firmware application failed
    OTA_NOT_CAPABLE,                ///< Portenta bootloader too old or not OTA-capable
    OTA_BEGIN_FAILED,               ///< OTA initialization failed
    OTA_WIFI_FW_MISSING,            ///< Arduino OPTA Wi-Fi firmware not installed
    PLATFORM_NOT_SUPPORTED,         ///< Board platform not supported by library
    BUFFER_OVERFLOW_REQUEST,        ///< Buffer overflow while building JSON request
    BUFFER_OVERFLOW_RESPONSE,      	///< Buffer overflow while reading server response
    CERTIFICATE_MISSING,            ///< Root CA certificate not found
    SERVER_ERROR_4XX                ///< Server returned HTTP 4xx error (check credentials)
};

/**
 * @typedef POTACallback
 * @brief Function signature for OTA availability notification
 * 
 * Callback is invoked when the server notifies that a new firmware
 * version is available via WebSocket push notification.
 * 
 * @param version String containing the available firmware version
 * 
 * Example:
 * @code
 * pota.onOTAAvailable([](const char* version) {
 *     Serial.print("New firmware available: ");
 *     Serial.println(version);
 *     // Optionally call checkAndPerformOTA() here
 * });
 * @endcode
 */
typedef void (*POTACallback)(const char* version);

// ========================================
// PLATFORM-SPECIFIC CLIENT DEFINITIONS
// ========================================

#ifdef ESP32
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
    #define NUSOCK_FULL_COMPLIANCE
    #include "utility/NuSock/NuSock.h"
    #define WIFI_CLIENT_TYPE WiFiClientSecure

#elif defined(ESP8266)
    #include <WiFiClientSecure.h>
    
    // Use NuSock library for more stable SSL WebSocket connections
    #define NUSOCK_FULL_COMPLIANCE
    #include "utility/NuSock/NuSock.h"
    #define WIFI_CLIENT_TYPE WiFiClientSecure
    
#elif defined(ARDUINO_OPTA)
    #include <WiFi.h>
    
    // Enable full socket compliance for NuSock library
    #define NUSOCK_FULL_COMPLIANCE
    #include "utility/NuSock/NuSock.h"
    #define WIFI_CLIENT_TYPE WiFiSSLClient

#else
    #error "Unsupported platform! Please compile for ESP32, ESP8266 or Arduino Opta."
#endif

#if defined(ARDUINO_OPTA) || defined(ESP8266) || defined(ESP32)
    /**
     * @enum WStype_t
     * @brief WebSocket event types (Arduino Opta and ESP8266 NuSock compatibility)
     * 
     * Defines event types for WebSocket communication.
     * Used internally for cross-platform WebSocket handling.
     */
    typedef enum {
        WStype_ERROR,               ///< WebSocket error occurred
        WStype_DISCONNECTED,        ///< WebSocket disconnected
        WStype_CONNECTED,           ///< WebSocket successfully connected
        WStype_TEXT,                ///< Received text message
        WStype_BIN,                 ///< Received binary message
        WStype_FRAGMENT_TEXT_START, ///< Text fragment start (unused)
        WStype_FRAGMENT_BIN_START,  ///< Binary fragment start (unused)
        WStype_FRAGMENT,            ///< Fragment continuation (unused)
        WStype_FRAGMENT_FIN,        ///< Fragment end (unused)
        WStype_PING,                ///< Ping frame received
        WStype_PONG,                ///< Pong frame received
    } WStype_t;
#endif


/**
 * @class POTA
 * @brief Main class for secure OTA updates and dashboard management
 * 
 * This class provides:
 * - Secure firmware update checks with HMAC-SHA256 verification
 * - WebSocket-based real-time dashboard communication
 * - Platform-agnostic OTA update mechanisms
 * - Optional Wi-Fi management and auto-reconnection
 * - Event-driven firmware update notifications
 * 
 * Typical initialization flow:
 * 1. Create POTA instance
 * 2. Call begin() with or without Wi-Fi credentials
 * 3. Set up dashboard widgets via setWidgetConfigCallback()
 * 4. Call loop() in main loop
 * 
 * Security features:
 * - All API communications use HTTPS with certificate validation
 * - Server responses verified with HMAC-SHA256 tokens
 * - WebSocket connections authenticated with JWT
 */
class POTA {
public:
    /**
     * @brief Default constructor
     * 
     * Initializes internal state and configures dashboard send handler.
     */
    POTA();
    
    /**
     * @brief Converts POTAError enum to human-readable string
     * 
     * Useful for error logging and debugging.
     * 
     * Example:
     * @code
     * POTAError err = pota.begin(...);
     * if (err != POTAError::SUCCESS) {
     *     Serial.println(POTA::errorToString(err));
     * }
     * @endcode
     * 
     * @param err The POTAError value to convert
     * @return const char* describing the error
     */
    static const char* errorToString(POTAError err);
    
    /**
     * @brief Initializes POTA library with optional Wi-Fi management
     * 
     * This method can work in two modes:
     * 
     * Mode 1 - POTA manages Wi-Fi (ssid and password provided):
     * 1. Connects to Wi-Fi network
     * 2. Configures auto-reconnection
     * 3. Sets up POTA with HTTPS client
     * 
     * Mode 2 - User manages Wi-Fi (ssid and password omitted):
     * 1. Assumes Wi-Fi is already connected
     * 2. Only sets up POTA configuration
     * 
     * In both modes:
     * - Configures TLS/SSL with root CA certificate
     * - Stores authentication credentials
     * - Initializes WebSocket connection for dashboard
     * 
     * Example with Wi-Fi management:
     * @code
     * POTAError err = pota.begin("ESP32", "1.0.0", 
     *                            "your_auth_token", "your_secret",
     *                            "WiFi_SSID", "WiFi_Password");
     * @endcode
     * 
     * Example without Wi-Fi management:
     * @code
     * WiFi.begin(ssid, password);
     * while (WiFi.status() != WL_CONNECTED) delay(500);
     * 
     * POTAError err = pota.begin("ESP32", "1.0.0", 
     *                            "your_auth_token", "your_secret");
     * @endcode
     * 
     * @param deviceType Device identifier (e.g., "ESP32", "Arduino_Opta")
     * @param firmwareVersion Current firmware version (e.g., "1.0.0")
     * @param authToken Authentication token from POTA service
     * @param serverSecret Secret key for HMAC-SHA256 generation
     * @param ssid Wi-Fi network SSID (optional, nullptr = user manages Wi-Fi)
     * @param password Wi-Fi network password (optional, required if ssid provided)
     * @return POTAError::SUCCESS on success, specific error code otherwise
     * 
     * @note If ssid is provided but password is missing, returns PARAMETER_INVALID_PASSWORD
     * @note Blocks until Wi-Fi connection succeeds or times out (30s) when managing Wi-Fi
     */
    POTAError begin(const char* deviceType,
                    const char* firmwareVersion,
                    const char* authToken,
                    const char* serverSecret,
                    const char* ssid = nullptr,
                    const char* password = nullptr,
                    const char* apiHost = "www.pleasedontcode.com");
    
    /**
     * @brief Retrieves platform-specific secure device identifier
     * 
     * Implementation varies by platform:
     * - ESP32: Uses eFuse MAC address (factory-programmed, tamper-resistant)
     * - ESP8266: Uses Wi-Fi interface MAC address
     * - Arduino Opta: Uses board information structure MAC
     * 
     * @return MAC address formatted as "XX:XX:XX:XX:XX:XX" (uppercase hex)
     * 
     * @note MAC address is used as unique device identifier for POTA API
     */
    String getSecureMACAddress();
    
    /**
     * @brief Checks for firmware updates and applies if available
     * 
     * This is a convenience method combining:
     * 1. checkOTAUpdate() - Query API for available updates
     * 2. performOTA() - Download and flash new firmware
     * 
     * Process:
     * 1. Sends device info to POTA API (MAC, type, version)
     * 2. Verifies server response with HMAC-SHA256
     * 3. Downloads firmware binary if update available
     * 4. Flashes new firmware using platform-specific OTA
     * 5. Reboots device with new firmware
     * 
     * Example:
     * @code
     * POTAError err = pota.checkAndPerformOTA();
     * if (err == POTAError::SUCCESS) {
     *     // Never reached - device reboots
     * } else if (err == POTAError::NO_UPDATE_AVAILABLE) {
     *     Serial.println("Already up-to-date");
     * } else {
     *     Serial.print("OTA failed: ");
     *     Serial.println(POTA::errorToString(err));
     * }
     * @endcode
     * 
     * @return POTAError::SUCCESS (never returns on successful update - device reboots)
     * @return POTAError::NO_UPDATE_AVAILABLE if firmware is up-to-date
     * @return Specific error code if check or update fails
     * 
     * @warning This function reboots the device on successful update!
     * @note Requires active internet connection
     */
    POTAError checkAndPerformOTA();
    
    /**
     * @brief Executes platform-specific OTA update process
     * 
     * Platform implementations:
     * - ESP32: Uses esp_https_ota library
     * - ESP8266: Uses ESP8266httpUpdate library
     * - Arduino Opta: Uses Arduino_Portenta_OTA library (download, decompress, apply)
     * 
     * @warning This function reboots the device on successful update!
     * 
     * @param OTA_file_url HTTPS URL of firmware binary (.bin or .ota)
     * @return POTAError::SUCCESS (never returns on success),
     *         or specific OTA failure code
     */
    POTAError performOTA(const char* OTA_file_url);
    
    /**
     * @brief Registers callback for OTA availability notifications
     * 
     * The callback is invoked when:
     * - Server sends "ota_available" WebSocket message
     * - A new firmware version is published on POTA dashboard
     * - Real-time push notification (no polling needed)
     * 
     * Callback receives the new firmware version string.
     * User can then decide whether to call checkAndPerformOTA().
     * 
     * Example:
     * @code
     * pota.onOTAAvailable([](const char* version) {
     *     Serial.print("New version available: ");
     *     Serial.println(version);
     *     
     *     // Option 1: Auto-update
     *     pota.checkAndPerformOTA();
     *     
     *     // Option 2: User confirmation
     *     if (userWantsUpdate()) {
     *         pota.checkAndPerformOTA();
     *     }
     * });
     * @endcode
     * 
     * @param callback Function to call when update available (nullptr to disable)
     * 
     * @note Callback is invoked from loop() context (safe for Serial, WiFi, etc.)
     * @note WebSocket must be connected to receive notifications
     */
    void onOTAAvailable(POTACallback callback) {_otaAvailableCallback = callback;}
    
    /**
     * @brief Main loop function for background POTA operations
     * 
     * This function should be called from Arduino loop().
     * It handles:
     * 
     * 1. WebSocket Management:
     *    - Auto-reconnection with exponential backoff (if enabled)
     *    - Event processing (ping/pong, messages)
     *    - Connection health monitoring
     * 
     * 2. Dashboard Synchronization:
     *    - Sends pending widget value updates
     *    - Transmits chart data points
     *    - Handles value quantization
     * 
     * 3. OTA Notifications:
     *    - Processes incoming "ota_available" messages
     *    - Triggers user callback if registered
     * 
     * Example:
     * @code
     * void loop() {
     *     pota.loop();  // Handle WebSocket, dashboard, and OTA notifications
     *     
     *     // Your application code
     *     float temp = readTemperature();
     *     pota.dashboard.setValue(tempWidgetID, temp);
     * }
     * @endcode
     * 
     * @param enableAutoReconnect Enable automatic WebSocket reconnection (default: true)
     * 
     * @note Non-blocking - returns immediately
     * @note If auto-reconnect disabled, WebSocket won't reconnect after disconnect
     * @note Reconnection attempts have 20-second cooldown to prevent flooding
     */
    void loop(bool enableAutoReconnect = true);
    
    /**
     * @brief WebSocket event handler (internal callback)
     * 
     * Processes WebSocket events from underlying library:
     * - CONNECTED: Updates status, notifies dashboard
     * - DISCONNECTED: Clears status
     * - BIN: Decodes MessagePack messages (system commands + dashboard data)
     * - ERROR: Logs and clears connection
     * - PING/PONG: Heartbeat mechanism
     * 
     * System commands intercepted:
     * - "ota_available": Triggers user OTA callback
     * 
     * Dashboard data forwarded to dashboard.decodeEventData()
     * 
     * @param type Event type (platform-specific enum cast to uint8_t)
     * @param payload Event data pointer
     * @param length Payload size in bytes
     * 
     * @note This is public for technical reasons (C callback bridge)
     * @note Users should never call this directly
     */
    void remoteWebSocketEvent(uint8_t type, uint8_t * payload, size_t length);
    
    /**
     * @brief Performs safe platform-specific system restart
     * 
     * Platform behavior:
     * - ESP32/ESP8266: Uses ESP.restart()
     * - Arduino Opta: Shuts down WiFi properly before NVIC_SystemReset()
     * 
     * On Opta, this prevents WiFi connection issues after reboot,
     * especially with mobile hotspots that have aggressive power saving.
     * 
     * @note This function never returns - device will reboot
     */
     void restart();

    
    /**
     * @var dashboard
     * @brief Public dashboard instance for widget management
     * 
     * Use this object to:
     * - Create widgets: dashboard.addWidget()
     * - Update values: dashboard.setValue()
     * - Register callbacks: dashboard.onUpdate()
     * - Set chart data: dashboard.setX(), dashboard.setY()
     * 
     * Example:
     * @code
     * uint8_t tempID = pota.dashboard.addWidget(TEMPERATURE_CARD, "Temperature");
     * pota.dashboard.setValue(tempID, 25.5f);
     * @endcode
     */
    potaDashboard dashboard;

private:
    // ========================================
    // CALLBACK AND STATE MANAGEMENT
    // ========================================
    POTACallback _otaAvailableCallback = nullptr; ///< User callback for OTA notifications
    bool _wsConnected = false;                    ///< WebSocket connection status
    bool _wsConnecting = false;                   ///< WebSocket connection in progress
    
    potaDashboard* _dashboard = &dashboard;       ///< Pointer to dashboard instance
    
    // ========================================
    // AUTHENTICATION AND DEVICE INFO
    // ========================================
    char _deviceType[32];        ///< Device type identifier
    char _firmwareVersion[32];   ///< Current firmware version
    char _authToken[64];         ///< Authentication token from POTA service
    char _serverSecret[65];      ///< Secret key for HMAC-SHA256 generation
    char _apiHost[64];           ///< POTA API host (default www.pleasedontcode.com)
    
   /**
     * @brief Core HMAC-SHA256 calculation function
     * 
     * Platform-agnostic implementation using:
     * - mbedTLS (ESP32, Arduino Opta)
     * - BearSSL (ESP8266)
     * 
     * Generates 64-character hexadecimal HMAC signature.
     * 
     * @param message String to sign (e.g., "field1:field2:field3")
     * @param secret HMAC secret key (server_secret)
     * @param outToken Output buffer for hex HMAC (must be >= 65 bytes)
     * @param outTokenSize Size of output buffer
     * @return POTAError::SUCCESS or generation error
     */
    POTAError _calculateHMAC(const char* message, 
                                  const char* secret, 
                                  char* outToken, 
                                  size_t outTokenSize);

    /**
     * @brief Generates HMAC token for OTA update verification
     * 
     * Constructs signature from OTA response fields:
     * "update:version:url:checksum:protocol_version:notes:timestamp"
     * 
     * Server must generate identical token for verification to pass.
     * This prevents man-in-the-middle attacks and ensures data integrity.
     * 
     * @param update Whether update is available (true/false)
     * @param version Firmware version string
     * @param url OTA download URL (must be HTTPS)
     * @param checksum Firmware file checksum
     * @param protocol_version POTA protocol version
     * @param notes Release notes
     * @param timestamp Server timestamp (Unix epoch string)
     * @param secret HMAC secret key
     * @param outToken Output buffer (>= 65 bytes)
     * @param outTokenSize Output buffer size
     * @return POTAError indicating success or failure
     */
    POTAError generateOtaServerToken(bool update,
                                  const char* version,
                                  const char* url,
                                  const char* checksum,
                                  const char* protocol_version,
                                  const char* notes,
                                  const char* timestamp,
                                  const char* secret,
                                  char* outToken, size_t outTokenSize);
                                  
    /**
     * @brief Generates HMAC token for WebSocket connection verification
     * 
     * Constructs signature from WebSocket credentials:
     * "success:ws_host:ws_path:device_id:timestamp"
     * 
     * The path includes JWT authentication token. This prevents
     * unauthorized WebSocket connections.
     * 
     * @param success Literal "True" or "False" from server
     * @param ws_host WebSocket hostname (e.g., "ws.pleasedontcode.com")
     * @param ws_path WebSocket path including JWT (e.g., "/ws/device/?jwt=...")
     * @param device_id Device MAC address
     * @param timestamp Server timestamp
     * @param secret HMAC secret key
     * @param outToken Output buffer (>= 65 bytes)
     * @param outTokenSize Output buffer size
     * @return POTAError indicating success or failure
     */
    POTAError generateWsServerToken(const char* success,
                                          const char* ws_host,
                                          const char* ws_path,
                                          const char* device_id,
                                          const char* timestamp,
                                          const char* secret,
                                          char* outToken, size_t outTokenSize);
    
    /**
     * @brief Ensures WebSocket connection is active
     * 
     * If not connected:
     * 1. Creates WebSocket client object (one-time)
     * 2. Fetches WebSocket URL and JWT from API
     * 3. Initiates SSL/TLS connection
     * 
     * Uses non-blocking approach to prevent loop() blocking.
     * 
     * @return POTAError::SUCCESS if connected/connecting, error otherwise
     */
    POTAError ensureWebSocketConnection();

    /**
     * @brief Checks POTA API for available firmware updates
     * 
     * Performs HTTPS request to check_update endpoint:
     * 1. Sends device info (MAC, type, version)
     * 2. Receives OTA metadata (version, URL, checksum)
     * 3. Verifies server HMAC token
     * 4. Returns download URL if update available
     * 
     * @param outOTAUrl Buffer to store OTA download URL
     * @param outOTAUrlSize Size of output buffer
     * @return POTAError::SUCCESS if update available,
     *         POTAError::NO_UPDATE_AVAILABLE if up-to-date,
     *         or specific error code
     */
    POTAError checkOTAUpdate(char* outOTAUrl, size_t outOTAUrlSize);
    
    /**
     * @brief Fetches WebSocket connection details from API
     * 
     * Performs HTTPS request to get_ws_token endpoint:
     * 1. Sends device authentication
     * 2. Receives WebSocket host, path, and JWT
     * 3. Verifies server HMAC token
     * 4. Returns verified connection parameters
     * 
     * @param outWSHost Buffer for WebSocket hostname
     * @param outWSHostSize Size of host buffer
     * @param outWSPath Buffer for WebSocket path (includes JWT)
     * @param outWSPathSize Size of path buffer
     * @return POTAError::SUCCESS if credentials received and verified,
     *         or specific error code
     */
    POTAError getWebSocketUrl(char* outWSHost, size_t outWSHostSize, 
                              char* outWSPath, size_t outWSPathSize);
    
    /**
    * @brief Performs complete Wi-Fi reset
    * 
    * Sequence:
    * 1. Disconnects from network
    * 2. Turns off Wi-Fi radio
    * 3. Re-enables in station mode
    * 
    * Used internally before new Wi-Fi connection attempts.
    */
    void resetWiFi();    
};