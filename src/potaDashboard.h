/*
  potaDashboard.h - POTA Dashboard Header
  ----------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/please-over-the-air
  
  Part of the POTA (Please Over The Air) Library
  
  This header defines the public API for the POTA Dashboard system,
  which enables real-time widget-based UI synchronization between
  embedded devices and web/mobile clients.
  
  Key Components:
  - Widget type definitions (cards, charts, controls)
  - Configuration and runtime state structures
  - Callback mechanisms for user interactions
  - Variant-based type-safe data handling 
  
*/

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>

// Platform compatibility layer for std::variant
// Older compilers (C++14 and below) and Arduino Opta require backport
#if defined(ARDUINO_OPTA) || __cplusplus < 201703L
    #include "utility/mpark/variant.hpp"
    namespace std {
        using mpark::variant;
        using mpark::visit;
        using mpark::holds_alternative;
        using mpark::get;
        using mpark::get_if;
        using mpark::monostate;
    }
#else
    #include <variant>
#endif

/**
 * @enum WidgetType
 * @brief Enumeration of all available widget types
 * 
 * Widgets are categorized into:
 * - Display widgets (show data): GENERIC_CARD, TEMPERATURE_CARD, etc.
 * - Control widgets (user input): TOGGLE_BUTTON_CARD, SLIDER_CARD, etc.
 * - Chart widgets (data visualization): BAR_CHART, LINE_CHART, etc.
 * - Special widgets: TAB (container), SEPARATOR_CARD (visual divider)
 */
typedef enum {
	UNDEFINED								= 0,   ///< Uninitialized widget type
	TAB 												= 1,   ///< Tab container for organizing widgets
    GENERIC_CARD 						= 2,   ///< Generic display card with customizable content
    TEMPERATURE_CARD 			= 3,   ///< Temperature display with icon and unit
    HUMIDITY_CARD 						= 4,   ///< Humidity percentage display
    AIR_CARD 									= 5,   ///< Air quality indicator
    ENERGY_CARD 						= 6,   ///< Energy/power consumption display
    FEEDBACK_CARD 					= 7,   ///< Status feedback with state colors
    PROGRESS_CARD 					= 8,   ///< Progress bar (0-100%)
    SEPARATOR_CARD 					= 9,   ///< Visual separator/divider
    TOGGLE_BUTTON_CARD 		= 10,  ///< On/Off toggle switch
    SLIDER_CARD 							= 11,  ///< Numeric slider with min/max/step
    JOYSTICK_2D_CARD 				= 12,  ///< 2D joystick (X and Y axes)
    JOYSTICK_X_CARD 					= 13,  ///< Horizontal joystick (X-axis only)
    JOYSTICK_Y_CARD 					= 14,  ///< Vertical joystick (Y-axis only)
    LINK_CARD 								= 15,  ///< Clickable hyperlink
    IMAGE_CARD 							= 16,  ///< Image display
    FILE_UPLOAD_CARD 				= 17,  ///< File upload interface
    PUSH_BUTTON_CARD 				= 18,  ///< Momentary push button
    ACTION_BUTTON_CARD 			= 19,  ///< Action trigger button (no state feedback)
    TIME_SYNC_CARD 					= 20,  ///< Time synchronization widget
    WEEK_SELECTION_CARD 		= 21,  ///< Day-of-week selector
    INDICATOR_BUTTON_CARD 	= 22,  ///< Button with state indicator
    RANGE_SLIDER_CARD 			= 23,  ///< Dual-handle range slider (min/max)
    DROPDOWN_CARD 					= 24,  ///< Dropdown selection menu
    TEXT_INPUT_CARD 					= 25,  ///< Text input field
    PASSWORD_CARD 					= 26,  ///< Password input (masked)
    COLOR_PICKER_CARD 			= 27,  ///< Color selection widget
    TIME_DIFFERENCE_CARD 		= 28,  ///< Time difference calculator
    BAR_CHART 								= 29,  ///< Vertical/horizontal bar chart
    LINE_CHART 								= 30,  ///< Line graph
    AREA_CHART 							= 31,  ///< Area chart (filled line graph)
    PIE_CHART 								= 32,  ///< Pie chart
    DOUGHNUT_CHART 				= 33   ///< Doughnut chart (pie with center hole)
} WidgetType;

/**
 * @name Status Constants
 * @brief Status codes for FEEDBACK_CARD and INDICATOR_BUTTON_CARD
 * 
 * These map to visual indicators (colors) on the dashboard:
 * - NONE: Gray (neutral)
 * - INFO: Blue (informational)
 * - SUCCESS: Green (positive)
 * - WARNING: Yellow/Orange (caution)
 * - DANGER: Red (critical)
 */
#define STATUS_NONE    		0
#define STATUS_INFO    		1
#define STATUS_SUCCESS 	2
#define STATUS_WARNING 	3
#define STATUS_DANGER  	4

/**
 * @struct WidgetConfig
 * @brief Configuration structure for widget creation
 * 
 * This structure defines the static properties of a widget that are
 * sent to clients during initialization. These properties determine
 * how the widget appears and behaves in the UI.
 * 
 * @note After creation, most config properties cannot be changed.
 *       Use refreshLayout() to recreate widgets with new config.
 */
typedef struct {
    uint8_t widget_id = 0;        ///< Unique identifier (auto-assigned)
    WidgetType type = WidgetType::UNDEFINED;  ///< Widget type (required)
    const char* name = "";        ///< Display name/label
    const char* extra = "";       ///< Additional JSON configuration (optional)
    bool displayOn = true;        ///< Visibility flag (true = visible)
    uint8_t id_tab = 0;           ///< Parent tab ID (0 = main tab)
    float step = 1;               ///< Value increment step (for sliders/numeric inputs)
    float min = 0;                ///< Minimum allowed value
    float max = 100;              ///< Maximum allowed value
} WidgetConfig;

/**
 * @struct WidgetData
 * @brief Data structure passed to widget callbacks
 * 
 * When a widget receives an update from a client (user interaction),
 * this structure is populated and passed to the registered callback.
 * 
 * Different widget types populate different fields:
 * - SLIDER_CARD: value (float)
 * - TOGGLE_BUTTON_CARD: value (bool)
 * - JOYSTICK_2D_CARD: x, y, direction
 * - RANGE_SLIDER_CARD: min, max
 * 
 * The struct provides type-safe getter methods with automatic conversion
 * and warning messages for type mismatches.
 */
struct WidgetData {
    std::variant<std::monostate, int, float, bool, String> value; ///< Primary value (type depends on widget)
    float x = 0.0f;               ///< Joystick X-axis value
    float y = 0.0f;               ///< Joystick Y-axis value
    float min = 0.0f;             ///< Range slider minimum value
    float max = 0.0f;             ///< Range slider maximum value
    const char* direction = "idle"; ///< Joystick direction ("up", "down", "left", "right", "idle")

    /**
     * @brief Safely extracts integer value with type checking
     * 
     * Attempts to convert stored value to int. Prints warning
     * if type mismatch occurs. Supports automatic conversion from
     * float, bool, and String.
     * 
     * @return Integer value or 0 if empty
     */
    int getInt() const {
        if (!std::holds_alternative<int>(value)) {
            Serial.print("⚠️ WARNING: Expected int, got ");
            Serial.println(getTypeName());
        }
        
        if (std::holds_alternative<int>(value)) return std::get<int>(value);
        if (std::holds_alternative<float>(value)) return (int)std::get<float>(value);
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1 : 0;
        if (std::holds_alternative<String>(value)) return std::get<String>(value).toInt();
        return 0;
    }
    
    /**
     * @brief Safely extracts float value with type checking
     * 
     * @return Float value or 0.0 if empty
     */
    float getFloat() const {
        if (!std::holds_alternative<float>(value)) {
            Serial.print("⚠️ WARNING: Expected float, got ");
            Serial.println(getTypeName());
        }
        
        if (std::holds_alternative<float>(value)) return std::get<float>(value);
        if (std::holds_alternative<int>(value)) return (float)std::get<int>(value);
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1.0f : 0.0f;
        if (std::holds_alternative<String>(value)) return std::get<String>(value).toFloat();
        return 0.0f;
    }
    
    /**
     * @brief Safely extracts boolean value with type checking
     * 
     * @return Boolean value or false if empty
     */
    bool getBool() const {
        if (!std::holds_alternative<bool>(value)) {
            Serial.print("⚠️ WARNING: Expected bool, got ");
            Serial.println(getTypeName());
        }
        
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
        if (std::holds_alternative<int>(value)) return std::get<int>(value) != 0;
        if (std::holds_alternative<float>(value)) return std::get<float>(value) != 0.0f;
        if (std::holds_alternative<String>(value)) return std::get<String>(value).length() > 0;
        return false;
    }
    
    /**
     * @brief Safely extracts String value with type checking
     * 
     * @return String value or empty string if incompatible type
     */
    String getString() const {
        if (!std::holds_alternative<String>(value)) {
            Serial.print("⚠️ WARNING: Expected String, got ");
            Serial.println(getTypeName());
        }
        
        if (std::holds_alternative<String>(value)) return std::get<String>(value);
        return toString();
    }
    
    /**
     * @brief Returns human-readable type name for debugging
     * 
     * @return Type name as String ("int", "float", "bool", "String", "empty")
     */
    String getTypeName() const {
        if (std::holds_alternative<int>(value)) return "int";
        if (std::holds_alternative<float>(value)) return "float";
        if (std::holds_alternative<bool>(value)) return "bool";
        if (std::holds_alternative<String>(value)) return "String";
        return "empty";
    }
        
    /**
     * @brief Converts value to String representation for Serial.print
     * 
     * @return String representation of current value
     */
    String toString() const {
        if (std::holds_alternative<int>(value))    return String(std::get<int>(value));
        if (std::holds_alternative<float>(value))  return String(std::get<float>(value), 2);
        if (std::holds_alternative<bool>(value))   return std::get<bool>(value) ? "true" : "false";
        if (std::holds_alternative<String>(value)) return std::get<String>(value);
        return "";
    }
};

/**
 * @typedef WidgetCallback
 * @brief Function signature for widget event callbacks
 * 
 * User registers callbacks via onUpdate(id, callback). When a widget
 * receives interaction from a client, the callback is invoked with
 * the corresponding WidgetData.
 * 
 * Example:
 * @code
 * pota.dashboard.onUpdate(sliderID, [](WidgetData data) {
 *     float value = data.getFloat();
 *     Serial.println(value);
 * });
 * @endcode
 */
typedef std::function<void(WidgetData)> WidgetCallback;

/**
 * @struct WidgetRuntime
 * @brief Runtime state tracker for active widgets
 * 
 * Internal structure that maintains the current state of each widget.
 * Unlike WidgetConfig (static properties), this tracks dynamic data:
 * - Current value
 * - Chart axis data (X/Y arrays)
 * - Update flags (determines what needs transmission)
 * - Callback function pointer
 * 
 * @note This is managed internally by potaDashboard. Users typically
 *       interact with widgets via their ID, not direct struct access.
 */
typedef struct {
    uint8_t widget_id  = 0;                  ///< Matches WidgetConfig ID
    WidgetType type = WidgetType::UNDEFINED; ///< Cached widget type
    WidgetCallback callback = nullptr;       ///< User-defined event handler
    float step = 1;                          ///< Cached step value (for precision calculation)
    
    // Primary value storage (int, float, bool, or String)
    std::variant<std::monostate, int, float, bool, String> value;
    
    // Chart data arrays (supports int*, float*, or string labels)
    std::variant<std::monostate, int*, float*, const char* const*> x_axis;
    std::variant<std::monostate, int*, float*, const char* const*> y_axis;
    
    size_t x_size = 0;         ///< Number of elements in x_axis array
    size_t y_size = 0;         ///< Number of elements in y_axis array
    
    // Update flags (mark data for transmission)
    bool xNeedsUpdate = false; ///< X-axis data changed
    bool yNeedsUpdate = false; ///< Y-axis data changed
    bool needsUpdate = false;  ///< Primary value changed
} WidgetRuntime;

/**
 * @typedef PotaSendCallback
 * @brief Function signature for external send handler
 * 
 * The dashboard doesn't manage network/serial communication directly.
 * Instead, it relies on an external handler registered via setSendHandler().
 * This provides flexibility to use WebSocket, Serial, BLE, etc.
 * 
 * @param data Pointer to MessagePack binary data
 * @param len Length of data in bytes
 */
typedef std::function<void(uint8_t*, size_t)> PotaSendCallback;

/**
 * @class potaDashboard
 * @brief Main dashboard management class
 * 
 * Provides the complete API for:
 * - Widget creation and configuration
 * - Value updates and synchronization
 * - Event callback registration
 * - Chart data management (X/Y axes)
 * - Bi-directional communication with clients
 * 
 * Typical usage pattern:
 * 1. Register send handler: setSendHandler()
 * 2. Register widget config callback: setWidgetConfigCallback()
 * 3. Create widgets in callback: addWidget()
 * 4. Register event handlers: onUpdate()
 * 5. Update values: setValue(), setX(), setY()
 * 6. Call sendUpdates() in main loop
 */
class potaDashboard {	
	
	protected:
		std::vector<WidgetRuntime> _widgets;  ///< Collection of active widget states
		void (*_widgetConfigCallback)() = nullptr; ///< User's widget setup function
		
		/**
		 * @brief Internal helper to find widget by ID
		 * @param widget_id Target widget identifier
		 * @return Pointer to WidgetRuntime or nullptr if not found
		 */
		WidgetRuntime* getWidgetById(uint8_t widget_id);
		
		/**
		 * @brief Internal helper to transmit JSON document
		 * @param doc ArduinoJson document to serialize and send
		 * @return true if sent successfully
		 */
		bool send(JsonDocument& doc);
		
		/**
		 * @brief Internal helper to send all pending widget updates
		 * @param force If true, forces update of all widgets
		 * @return true if transmission succeeded
		 */
		bool sendAllWidgetRuntime(bool force = false);
		
		/**
		 * @brief Calculates decimal precision needed for a step value
		 * @param step Value increment (e.g., 0.1, 0.01, 1.0)
		 * @return Number of decimal places (0-5)
		 */
		int getPrecision(float step);
		
	private:
	    PotaSendCallback _externalSend = nullptr; ///< External send handler (WebSocket/Serial)
	    bool _isConnected = false;                ///< Connection status flag
		bool initWidgetConfig = false;            ///< One-time initialization flag
	    unsigned long _lastPingTime = millis();   ///< Timestamp of last received ping
	    static constexpr unsigned long PING_TIMEOUT_MS = 60000; ///< Consider dashboard inactive after 60s
	
	public:
	    /**
	     * @brief Registers external send handler for network transmission
	     * 
	     * The dashboard uses this callback to transmit MessagePack data.
	     * Must be set before any communication can occur.
	     * 
	     * Example:
	     * @code
	     * pota.dashboard.setSendHandler([](uint8_t* data, size_t len) {
	     *     webSocket.sendBIN(data, len);
	     * });
	     * @endcode
	     * 
	     * @param fn Callback function accepting binary data and length
	     */
	    void setSendHandler(PotaSendCallback fn) { _externalSend = fn; }
	    
	    /**
	     * @brief Updates connection status (called by POTA internally)
	     * 
	     * @param connected true if WebSocket/connection is active
	     */
	    void setConnectionStatus(bool connected) { _isConnected = connected; }
	    
		/**
		 * @brief Registers widget configuration callback
		 * 
		 * This callback is invoked when widgets need to be (re)initialized.
		 * User should define all widgets inside this function.
		 * 
		 * Example:
		 * @code
		 * pota.dashboard.setWidgetConfigCallback([]() {
		 *     uint8_t id = pota.dashboard.addWidget(SLIDER_CARD, "Brightness");
		 * });
		 * @endcode
		 * 
		 * @param fn Function pointer to configuration callback
		 */
		void setWidgetConfigCallback(void (*fn)());
		
		/**
		 * @brief Forces complete dashboard reinitialization
		 * 
		 * Clears all widgets and re-executes widget configuration callback.
		 * Useful for dynamic UI changes.
		 */
		void refreshLayout();
		
		/**
		 * @brief Registers callback for widget interactions
		 * 
		 * @param id Widget identifier returned by addWidget()
		 * @param cb Callback function to invoke on widget events
		 */
		void onUpdate(uint8_t id, WidgetCallback cb);
		
		/**
		 * @brief Creates new widget from complete configuration structure
		 * 
		 * @param config Pointer to WidgetConfig with all properties set
		 * @return Unique widget ID for future reference
		 */
		uint8_t addWidget(WidgetConfig* config);
		
	    /**
	     * @brief Creates new widget with individual parameters
	     * 
	     * Convenience method that constructs WidgetConfig internally.
	     * 
	     * @param type Widget type (required)
	     * @param name Display name (required)
	     * @param id_tab Parent tab (0 = main, optional)
	     * @param extra JSON config string (optional)
	     * @param step Value increment (optional, default 1.0)
	     * @param min Minimum value (optional, default 0.0)
	     * @param max Maximum value (optional, default 100.0)
	     * @return Unique widget ID
	     */
	    uint8_t addWidget(WidgetType type, const char* name, uint8_t id_tab = 0, const char* extra = "", float step = 1.0, float min = 0.0, float max = 100.0 );
	
	    /**
	     * @brief Updates widget value with automatic change detection
	     * 
	     * Performs quantization (for floats) and marks widget for
	     * transmission only if value actually changed.
	     * 
	     * @param widget_id Target widget
	     * @param newValue New value (int, float, bool, or String)
	     * @return true on success, false if widget not found
	     */
	    bool setValue(uint8_t widget_id, const std::variant<std::monostate, int, float, bool, String>& newValue);
	    
	    /**
	     * @brief Sets X-axis data for chart widgets
	     * 
	     * @param widget_id Target chart widget
	     * @param newX Pointer to array (int*, float*, or const char* const*)
	     * @param size Number of array elements
	     * @return true on success
	     */
	    bool setX(uint8_t widget_id, const std::variant<std::monostate, int*, float*, const char* const*>& newX, size_t size);
	    
	    /**
	     * @brief Sets Y-axis data for chart widgets
	     * 
	     * @param widget_id Target chart widget
	     * @param newY Pointer to array (int* or float*)
	     * @param size Number of array elements
	     * @return true on success
	     */
	    bool setY(uint8_t widget_id, const std::variant<std::monostate, int*, float*, const char* const*>& newY, size_t size);
	
	    /**
	     * @brief Internal: Sends widget configuration to server
	     * 
	     * Called automatically by addWidget(). Users typically don't
	     * need to call this directly.
	     * 
	     * @param config Widget configuration to transmit
	     * @return true if sent successfully
	     */
	    bool sendWidgetConfigToServer(WidgetConfig* config);
	    
	    /**
	     * @brief Processes incoming MessagePack events from clients
	     * 
	     * Called internally by POTA when WebSocket data arrives.
	     * Handles commands (ping, get:layout) and widget updates.
	     * 
	     * @param data Pointer to MessagePack binary data
	     * @param len Data length in bytes
	     */
	    void decodeEventData(uint8_t * data, size_t len);
	    
	    /**
	     * @brief Main update loop function
	     * 
	     * Call this regularly from main loop() to:
	     * - Initialize widgets on first run
	     * - Transmit pending updates to clients
	     * - Check connection health
	     * 
	     * @param force If true, forces update of all widgets
	     */
	    void sendUpdates(bool force = false);
};