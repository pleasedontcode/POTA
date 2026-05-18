/*
  potaDashboard.cpp - POTA Dashboard Implementation
  --------------------------------------------------
  Author: Francesco Alessandro Colucci (pleasedontcode.com)
  License: MIT (see LICENSE file in the root of this project)
  Repository: https://github.com/pleasedontcode/POTA
  Website/Service: https://www.pleasedontcode.com/programming-over-the-air
  
  Part of the POTA (Programming Over The Air) Library
  
  This file implements the dashboard functionality for real-time widget
  management and data synchronization between embedded devices and web clients.
  
  Key Features:
  - Widget configuration and runtime management
  - Bi-directional data synchronization via MessagePack
  - Automatic value quantization and precision handling
  - Chart data (X/Y axis) management
  - Event-driven callback system
  
*/

#include "potaDashboard.h"

// Pre-encoded MessagePack "pong" response for ping/pong heartbeat mechanism
// This avoids runtime encoding overhead for frequent ping responses
static const uint8_t PONG_MSG_PACK[] = {0x81, 0xA7, 0x63, 0x6F, 0x6D, 0x6D, 0x61, 0x6E, 0x64, 0xA4, 0x70, 0x6F, 0x6E, 0x67};

/**
 * @brief Sends widget configuration to the server
 * 
 * Serializes widget configuration into MessagePack format and transmits it
 * to the connected client. This defines the widget's structure, type, and
 * display properties.
 * 
 * @param config Pointer to WidgetConfig structure containing widget metadata
 * @return true if configuration was successfully sent, false otherwise
 */
bool potaDashboard::sendWidgetConfigToServer(WidgetConfig* config) {

	// Create JSON document with appropriate size based on ArduinoJson version
	#if ARDUINOJSON_VERSION_MAJOR == 6
	  DynamicJsonDocument doc(DASH_JSON_DOCUMENT_ALLOCATION);
	#else
	  JsonDocument doc;
	#endif
	
	// Build configuration command structure
	doc["command"] = "addWidget";
	JsonObject cfg = doc.createNestedObject("WidgetConfig");
	cfg["id"]       			= config->widget_id;
	cfg["type"]      		= static_cast<uint8_t>(config->type);
	cfg["name"]      	= config->name;        // char[32]
	cfg["displayOn"] 	= config->displayOn;
	cfg["id_tab"]    		= config->id_tab;
	cfg["extra"]       = config->extra;
	
	// Calculate precision once based on step value to avoid binary noise
    // This ensures consistent decimal representation (e.g., 0.01 instead of 0.00999999)
    int p = getPrecision(config->step);
    
    // Format float values as strings with calculated precision
    cfg["step"] = String(config->step, p);
    cfg["min"]  = String(config->min, p);
    cfg["max"]  = String(config->max, p);
	
	if(!send(doc)) return false;
	
	return true;
}

/**
 * @brief Convenience method to add a widget with individual parameters
 * 
 * This overload allows users to create widgets without manually constructing
 * a WidgetConfig structure. Parameters are packaged internally.
 * 
 * @param type Widget type (e.g., SLIDER_CARD, TOGGLE_BUTTON_CARD)
 * @param name Display name for the widget
 * @param id_tab Tab ID where widget should appear (0 for main tab)
 * @param extra Additional JSON configuration string
 * @param step Value increment step (affects precision)
 * @param min Minimum value constraint
 * @param max Maximum value constraint
 * @return Unique widget ID assigned by the system
 */
uint8_t potaDashboard::addWidget(WidgetType type, const char* name, uint8_t id_tab, const char* extra, float step, float min, float max ) {
    WidgetConfig cfg; // Create temporary config structure
    cfg.type = type;
    cfg.name = name;
    cfg.id_tab = id_tab;
    cfg.extra = extra;
    cfg.step = step;
    cfg.min = min;
    cfg.max = max;
    cfg.displayOn = true;

    return addWidget(&cfg); // Forward to main implementation
}

/**
 * @brief Registers a new widget and initializes its runtime state
 * 
 * Core widget registration function that:
 * 1. Assigns a unique ID
 * 2. Creates runtime tracking structure
 * 3. Sends configuration to connected clients
 * 
 * @param config Pointer to complete widget configuration
 * @return Unique widget ID for future reference
 */
uint8_t potaDashboard::addWidget(WidgetConfig* config) {
    // 1️⃣ Assign unique ID based on current widget count
    uint8_t newId = _widgets.size() + 1;
    config->widget_id = newId;
    
    // 2️⃣ Create corresponding runtime state tracker
    WidgetRuntime w;
    w.widget_id = newId;
    w.type = config->type;
    //w.value not initialized (uses std::monostate by default)
    w.step = config->step;
    w.x_axis = (int*)nullptr;
    w.y_axis = (int*)nullptr;
    w.x_size = 0;
    w.y_size = 0;
    w.xNeedsUpdate = false;
    w.yNeedsUpdate = false;
    w.needsUpdate = false;

    _widgets.push_back(w);
    
    // 3️⃣ Transmit configuration to server (defines widget structure)
    sendWidgetConfigToServer(config);

    return newId;
}

/**
 * @brief Retrieves widget runtime state by ID
 * 
 * @param widget_id Unique identifier assigned during widget creation
 * @return Pointer to WidgetRuntime if found, nullptr otherwise
 */
WidgetRuntime* potaDashboard::getWidgetById(uint8_t widget_id) {
    for (auto &w : _widgets) {
        if (w.widget_id == widget_id)
            return &w;
    }
    return nullptr;
}

/**
 * @brief Updates widget value with automatic quantization and change detection
 * 
 * This function:
 * - Quantizes float values to step boundaries (prevents noise)
 * - Detects actual value changes (avoids unnecessary updates)
 * - Marks widget for transmission if value changed
 * 
 * @param widget_id Target widget identifier
 * @param newValue New value (supports int, float, bool, String via std::variant)
 * @return true if update succeeded, false if widget not found
 */
bool potaDashboard::setValue(uint8_t widget_id, const std::variant<std::monostate, int, float, bool, String>& newValue) {
    WidgetRuntime* w = getWidgetById(widget_id);
    if (!w) return false;

    if (std::holds_alternative<float>(newValue)) {
        float incomingVal = std::get<float>(newValue);
        
        // 1. Step-based quantization to prevent floating-point drift
        if (w->step > 0) {
            incomingVal = round(incomingVal / w->step) * w->step;
        }

        // 2. Change detection: only update if value actually changed
        if (std::holds_alternative<float>(w->value)) {
            if (std::get<float>(w->value) == incomingVal) return true; // No real change
        }
        
        w->value = incomingVal;
    } else {
        // Generic change detection for other types (int, bool, String)
        if (w->value == newValue) return true; 
        w->value = newValue;
    }
    
    w->needsUpdate = true; // Mark for transmission in next update cycle
    return true;
}

/**
 * @brief Calculates optimal decimal precision based on step value
 * 
 * Determines how many decimal places are needed to represent the step
 * without trailing zeros. This ensures clean display formatting.
 * 
 * Examples:
 * - step = 1.0   → precision = 0 (integer)
 * - step = 0.1   → precision = 1
 * - step = 0.01  → precision = 2
 * 
 * @param step The increment value used for the widget
 * @return Number of decimal places needed (0-5)
 */
int potaDashboard::getPrecision(float step) {
    if (step <= 0) return 2; // Default precision for invalid step
    
    // If step is a whole number (e.g., 5.0, 10.0, 1.0), use integer precision
    if (step == (float)((int)step)) return 0;

    String s = String(step, 7); // Convert to string with high precision
    s.remove(s.length() - 1);   // Remove potential rounding artifacts
    
    // Strip trailing zeros: "5.500000" → "5.5"
    while (s.endsWith("0")) s.remove(s.length() - 1);
    
    // Find decimal point position
    int dotIndex = s.indexOf('.');
    if (dotIndex == -1) return 0;
    
    // Precision is the length after the decimal point
    int precision = s.length() - dotIndex - 1;
    
    // Cap at 5 to prevent excessive precision from float errors
    return (precision > 5) ? 5 : precision;
}

/**
 * @brief Sets X-axis data for chart widgets
 * 
 * Accepts arrays of int, float, or string labels for chart X-axis.
 * Supports time series, categories, or numerical ranges.
 * 
 * @param widget_id Target chart widget
 * @param newX Pointer to array data (int*, float*, or const char* const*)
 * @param size Number of elements in the array
 * @return true on success, false if widget not found
 */
bool potaDashboard::setX(uint8_t widget_id, const std::variant<std::monostate, int*, float*, const char* const*>& newX, size_t size) {
    WidgetRuntime* w = getWidgetById(widget_id);
    if (!w) return false;

    w->x_axis = newX;
    w->x_size = size;
    w->xNeedsUpdate = true; // Schedule X-axis data transmission

    return true;
}

/**
 * @brief Sets Y-axis data for chart widgets
 * 
 * Accepts arrays of int or float values for chart Y-axis.
 * Represents the actual data points to be plotted.
 * 
 * @param widget_id Target chart widget
 * @param newY Pointer to array data (int* or float*)
 * @param size Number of elements in the array
 * @return true on success, false if widget not found
 */
bool potaDashboard::setY(uint8_t widget_id, const std::variant<std::monostate, int*, float*, const char* const*>& newY, size_t size) {
    WidgetRuntime* w = getWidgetById(widget_id);
    if (!w) return false;

    w->y_axis = newY;
    w->y_size = size;
    w->yNeedsUpdate = true; // Schedule Y-axis data transmission

    return true;
}

/**
 * @brief Serializes and transmits a JSON document via MessagePack
 * 
 * Converts JSON to binary MessagePack format for efficient transmission.
 * Uses external send handler configured via setSendHandler().
 * 
 * @param doc ArduinoJson document to serialize
 * @return true if sent successfully, false if not connected or send failed
 */
bool potaDashboard::send(JsonDocument& doc) {
  if (!_isConnected || !_externalSend) {
    return false;
  }

  const size_t len = measureMsgPack(doc);
  
  // Allocate temporary buffer for MessagePack encoding
  uint8_t* buffer = (uint8_t*)malloc(len); 
  if (!buffer) {
        return false; // Allocation failed
  }

  size_t written = serializeMsgPack(doc, buffer, len);
  if (written == 0) {
    free(buffer);
    return false; // Serialization failed
  }

  // Transmit via external handler (WebSocket, Serial, etc.)
  _externalSend(buffer, written);

  free(buffer);
  doc.clear(); // Free JSON document memory
  
  return true;
}

/**
 * @brief Transmits all pending widget updates to clients
 * 
 * Core synchronization function that:
 * - Collects all widgets marked for update
 * - Excludes TAB widgets (structural only)
 * - Packages updates into efficient batch message
 * - Resets update flags after transmission
 * 
 * Uses std::visit pattern for type-safe variant handling.
 * 
 * @param force If true, forces update of all widgets regardless of flags
 * @return true if transmission succeeded, false otherwise
 */
bool potaDashboard::sendAllWidgetRuntime(bool force) {
    #if ARDUINOJSON_VERSION_MAJOR == 6
        DynamicJsonDocument doc(DASH_JSON_DOCUMENT_ALLOCATION);
    #else
        JsonDocument doc;
    #endif

    doc["command"] = "update:components";
    JsonArray arr = doc.createNestedArray("widgets");
    
    for (WidgetRuntime &wrt : _widgets) {
        // Exclude TABs from component updates (they're structural only)
        if (wrt.type == WidgetType::TAB) continue;

        // Force update mode: mark all data for transmission
        if(force) { 
            wrt.needsUpdate = true;
            wrt.xNeedsUpdate = true;
            wrt.yNeedsUpdate = true;
        }
            
        // Skip widgets with no pending changes
        if (!wrt.needsUpdate && !wrt.xNeedsUpdate && !wrt.yNeedsUpdate) 
            continue;

        // --- Prepare temporary buffer for this widget ---
        // We use a temporary object to verify we're actually adding data
        #if ARDUINOJSON_VERSION_MAJOR == 6
             StaticJsonDocument<512> itemDoc;
             JsonObject w = itemDoc.to<JsonObject>();
        #else
             JsonDocument itemDoc;
             JsonObject w = itemDoc.to<JsonObject>();
        #endif

        bool hasData = false; // Track if widget has any data to send
        
        // --- 1. VALUE UPDATE ---
        if (wrt.needsUpdate) {
            // Visitor pattern for type-safe variant handling
            struct ValueVisitor {
                WidgetRuntime& wrt;
                JsonObject& w;
                bool& hasData;
        
                void operator()(std::monostate) {} // No value set
                void operator()(int arg) {
                    hasData = true;
                    // Special handling for indicator/feedback cards (state + value)
                    if (wrt.type == WidgetType::INDICATOR_BUTTON_CARD || wrt.type == WidgetType::FEEDBACK_CARD) {
                        w["s"] = arg; // State
                        if (wrt.type == WidgetType::INDICATOR_BUTTON_CARD) w["v"] = arg; // Value
                    } else {
                        w["v"] = arg;
                    }
                }
                void operator()(float arg) {
                    hasData = true;
                    // Calculate precision based on step for clean display
                    int p = (wrt.step > 0 && wrt.step < 1) ? (wrt.step < 0.1 ? 2 : 1) : 0;
                    w["v"] = String(arg, p); // Format as string to avoid binary noise
                }
                void operator()(const String& arg) {
                    hasData = true;
                    w["v"] = arg;
                }
                void operator()(bool arg) {
                    hasData = true;
                    w["v"] = arg ? 1 : 0; // Convert to int for consistency
                }
            };
            ValueVisitor v{wrt, w, hasData};
            std::visit(v, wrt.value);
        }
        
        // --- 2. X AXIS UPDATE ---
        if (wrt.xNeedsUpdate) {
            // Visitor for X-axis data (supports int, float, string arrays)
            struct AxisVisitor {
                WidgetRuntime& wrt;
                JsonObject& w;
                bool& hasData;
        
                void operator()(std::monostate) {} // No data
                void operator()(float* arg) {
                    if (arg == nullptr || wrt.x_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("x");
                    for (size_t i = 0; i < wrt.x_size; i++) arr.add(String(arg[i], 2));
                }
                void operator()(int* arg) {
                    if (arg == nullptr || wrt.x_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("x");
                    for (size_t i = 0; i < wrt.x_size; i++) arr.add(arg[i]);
                }
                // Exhaustive handling for all string pointer types in variant
                void operator()(const char** arg) {
                    if (arg == nullptr || wrt.x_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("x");
                    for (size_t i = 0; i < wrt.x_size; i++) arr.add(arg[i]);
                }
                void operator()(const char* const* arg) {
                    if (arg == nullptr || wrt.x_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("x");
                    for (size_t i = 0; i < wrt.x_size; i++) arr.add(arg[i]);
                }
            };
            AxisVisitor v{wrt, w, hasData};
            std::visit(v, wrt.x_axis);
        }
        
        // --- 3. Y AXIS UPDATE ---
        if (wrt.yNeedsUpdate) {
            struct YAxisVisitor {
                WidgetRuntime& wrt;
                JsonObject& w;
                bool& hasData;
        
                void operator()(std::monostate) {} // No data
                void operator()(float* arg) {
                    if (arg == nullptr || wrt.y_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("y");
                    int p = (wrt.step > 0 && wrt.step < 1) ? (wrt.step < 0.1 ? 2 : 1) : 0;
                    for (size_t i = 0; i < wrt.y_size; i++) arr.add(String(arg[i], p));
                }
                void operator()(int* arg) {
                    if (arg == nullptr || wrt.y_size == 0) return;
                    hasData = true;
                    JsonArray arr = w.createNestedArray("y");
                    for (size_t i = 0; i < wrt.y_size; i++) arr.add(arg[i]);
                }
                // Y-axis doesn't support string labels
                void operator()(const char** arg) {}
                void operator()(const char* const* arg) {}
            };
            YAxisVisitor v{wrt, w, hasData};
            std::visit(v, wrt.y_axis);
        }
        
        // --- FINALIZATION: If we have data, add ID and insert into array ---
        if (hasData) {
            JsonObject finalW = arr.add<JsonObject>();
            finalW["id"] = wrt.widget_id;
            // Copy keys (v, s, x, y) from temporary object to final object
            for (JsonPair kv : w) {
                finalW[kv.key()] = kv.value();
            }
        }
        
        // Always reset flags to prevent update loops
        wrt.needsUpdate = wrt.xNeedsUpdate = wrt.yNeedsUpdate = false;
    }

    if (arr.size() == 0) return true; // No updates needed
    return send(doc);
}

/**
 * @brief Processes incoming MessagePack events from clients
 * 
 * Handles various command types:
 * - "ping" → responds with pong (heartbeat)
 * - "get:layout" → triggers widget reconfiguration
 * - "get:alldata" → forces full state resync
 * - Widget updates → processes user interactions
 * 
 * Implements automatic feedback mechanism for most widget types.
 * 
 * @param data Pointer to MessagePack binary data
 * @param len Length of data in bytes
 */
void potaDashboard::decodeEventData(uint8_t * data, size_t len) {
  
    #if ARDUINOJSON_VERSION_MAJOR == 6
        StaticJsonDocument<1024> json;
    #else
        JsonDocument json;
    #endif

    // Decode MessagePack binary format to JSON
    deserializeMsgPack(json, data, len);

    // --- HEARTBEAT MECHANISM ---
    // Server sends periodic pings to verify connection is alive
    if (json["command"] == "ping") {
        _lastPingTime = millis();  // Update timestamp for timeout detection
        if (_externalSend) _externalSend((uint8_t*)PONG_MSG_PACK, sizeof(PONG_MSG_PACK));
    } 
    // --- LAYOUT REQUEST ---
    // Client is requesting full widget configuration (e.g., after reconnect)
    else if (json["command"] == "get:layout") {
        _widgets.clear(); // Clear existing widgets
	    if (_widgetConfigCallback) _widgetConfigCallback();   // Trigger user's widget setup
	} 
	// --- DATA SYNC REQUEST ---
	// Client wants current state of all widgets
	else if(json["command"] == "get:alldata") {
	    sendAllWidgetRuntime(true); // Force send all widget data
	} 
	// --- WIDGET INTERACTION ---
	// Message contains widget update from user interaction
	else if (json.containsKey("id")) {
	    uint8_t id = json["id"];
        WidgetRuntime* w = getWidgetById(id);
        if (w) {
            WidgetData d; // Prepare data structure for callback
    
            // 1. EXTRACT DATA AND UPDATE RUNTIME STATE
            switch(w->type) {
                // --- JOYSTICK WIDGETS ---
                // Handle 2D joystick and single-axis variants
                case WidgetType::JOYSTICK_2D_CARD:
                case WidgetType::JOYSTICK_X_CARD:
                case WidgetType::JOYSTICK_Y_CARD:
                    d.x = json["x"].as<float>(); // Force float parsing
                    d.y = json["y"].as<float>();
                    d.direction = json["direction"] | "idle"; // Cardinal direction string
                    break;
    
                // --- RANGE SLIDER ---
                // Parses "min,max" value string
                case WidgetType::RANGE_SLIDER_CARD: {
                    String val = json["value"] | "0.0,0.0";
                    int comma = val.indexOf(',');
                    if (comma != -1) {
                        d.min = val.substring(0, comma).toFloat();
                        d.max = val.substring(comma + 1).toFloat();
                    }
                    w->value = val; // Store raw string in runtime
                    w->needsUpdate = true; // Enable automatic feedback
                    break;
                }
                
                // --- FILE UPLOAD ---
                // Receives file path/URL from client
                case WidgetType::FILE_UPLOAD_CARD: {
                    String val = json["value"] | "";
                    //w->value = val; // Optionally store in runtime
                    d.value = val; // Pass to callback
                    break;
                }
                
                // --- ACTION BUTTON ---
                // No feedback needed (fire-and-forget action)
                case WidgetType::ACTION_BUTTON_CARD: {
                	d.value = json["value"].as<bool>();
                    break; // do nothing - feedback not required
                }
                
    
                // --- GENERIC VALUE HANDLING ---
                // Handles all other widget types with standard value field
                default:
                   std::variant<std::monostate, int, float, bool, String> tempVal;
                   // Type detection and conversion
                    if (json["value"].is<bool>()) tempVal = json["value"].as<bool>();
                    else if (json["value"].is<int>()) tempVal = json["value"].as<int>();
                    else if (json["value"].is<float>()) tempVal = json["value"].as<float>();
                    else tempVal = json["value"].as<String>();
                    
                    // Only update if value actually changed (prevents infinite feedback loops)
                    if (w->value != tempVal) {
                        w->value = tempVal;
                        d.value = tempVal;
                        w->needsUpdate = true; // Schedule feedback transmission
                    } else {
                        d.value = w->value; // Pass current value to callback
                        w->needsUpdate = false; // No change, no feedback needed
                    }
                    break;
            }
    
            // 2. EXECUTE USER CALLBACK
            // Notify application code of widget interaction
            if (w->callback) {
                w->callback(d);
            }

            // 3. IMMEDIATE FEEDBACK TRANSMISSION
            // Send updated state back to client (for two-way sync)
            sendUpdates(false);
        }
	}
	
}

/**
 * @brief Registers a callback function for widget interactions
 * 
 * The callback will be invoked whenever the widget receives an update
 * from a client (e.g., user moved slider, toggled switch).
 * 
 * @param id Widget identifier
 * @param cb Callback function accepting WidgetData parameter
 */
void potaDashboard::onUpdate(uint8_t id, WidgetCallback cb) {
    WidgetRuntime* w = getWidgetById(id);
    if (w) w->callback = cb;
}

/**
 * @brief Main update loop - handles periodic synchronization
 * 
 * Should be called regularly from main loop(). This function:
 * - Initializes widget configuration on first run
 * - Checks connection health via ping timeout
 * - Transmits pending widget updates
 * 
 * @param force If true, forces update of all widgets
 */
void potaDashboard::sendUpdates(bool force) {
    if (!_isConnected) {
        return; // No active connection
    }
    
    // One-time initialization: trigger user's widget setup callback
    if(!initWidgetConfig) {
        if (_widgetConfigCallback) {
            _widgetConfigCallback();   // User defines widgets here
            initWidgetConfig = true;
        }
    }
    
    // Only send updates if we've received a recent ping (dashboard is active)
    if (millis() - _lastPingTime <= PING_TIMEOUT_MS) {
        sendAllWidgetRuntime(force);
    }
}

/**
 * @brief Registers the widget configuration callback
 * 
 * This callback is invoked when:
 * - Initial connection is established
 * - Client requests layout refresh
 * - User calls refreshLayout()
 * 
 * User should define all widgets inside this callback using addWidget().
 * 
 * @param fn Function pointer to configuration callback
 */
void potaDashboard::setWidgetConfigCallback(void (*fn)()) {
    _widgetConfigCallback = fn;
}

/**
 * @brief Forces complete dashboard reinitialization
 * 
 * Useful for:
 * - Dynamic UI changes based on device state
 * - Switching between different dashboard layouts
 * - Recovery from desync issues
 * 
 * Process:
 * 1. Clears all local widget state
 * 2. Re-executes widget configuration callback
 * 3. Sends fresh initial values to clients
 */
void potaDashboard::refreshLayout() {
    // 1. Clear local widget state
    _widgets.clear();

    // 2. Execute configuration callback (user recreates widgets)
    if (_widgetConfigCallback) {
        _widgetConfigCallback(); 
    }
    
    // 3. Immediately send initial values of new widgets
    sendAllWidgetRuntime(true);
}