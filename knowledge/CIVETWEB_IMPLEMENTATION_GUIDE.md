# CivetWeb Implementation Guide for AiO v26

## Overview
This guide provides step-by-step instructions for properly integrating CivetWeb into the AiO v26 project for WebSocket telemetry streaming.

## File Structure
```
lib/
├── civetweb/
│   ├── civetweb.c         # Main CivetWeb source (from test/civetweb/src/)
│   ├── civetweb.h         # CivetWeb header (from test/civetweb/include/)
│   └── library.json       # PlatformIO library config
└── aio_system/
    ├── WebManagerCivet.h  # Our wrapper for CivetWeb
    └── WebManagerCivet.cpp
```

## Configuration for Teensy 4.1

### 1. Create library.json
```json
{
  "name": "CivetWeb-Teensy",
  "version": "1.17.0",
  "description": "CivetWeb HTTP/WebSocket server for Teensy",
  "build": {
    "flags": [
      "-DNO_SSL",
      "-DNO_CGI", 
      "-DNO_FILES",
      "-DUSE_WEBSOCKET",
      "-DNO_LUA",
      "-DNO_DUKTAPE",
      "-DMAX_WORKER_THREADS=2",
      "-DUSE_STACK_SIZE=8192"
    ]
  }
}
```

### 2. WebManagerCivet.h
```cpp
#ifndef WEB_MANAGER_CIVET_H
#define WEB_MANAGER_CIVET_H

#include <civetweb.h>
#include <memory>

struct TelemetryPacket {
    uint32_t timestamp;
    float was_angle;
    float was_angle_target;
    int16_t encoder_count;
    float current_draw;
    float speed_kph;
    float heading;
    uint16_t status_flags;
    uint8_t steer_switch;
    uint8_t work_switch;
    uint8_t reserved[2];
} __attribute__((packed));

class WebManagerCivet {
public:
    WebManagerCivet();
    ~WebManagerCivet();
    
    bool begin(uint16_t port = 8081);
    void stop();
    void handleClient(); // Called from main loop
    
    // Broadcast telemetry to all WebSocket clients
    void broadcastTelemetry(const TelemetryPacket& packet);
    
private:
    struct mg_context* ctx;
    uint16_t serverPort;
    bool isRunning;
    
    // Static callbacks for CivetWeb
    static int httpHandler(struct mg_connection* conn, void* cbdata);
    static int wsConnectHandler(const struct mg_connection* conn, void* cbdata);
    static void wsReadyHandler(struct mg_connection* conn, void* cbdata);
    static int wsDataHandler(struct mg_connection* conn, int opcode, 
                           char* data, size_t data_len, void* cbdata);
    static void wsCloseHandler(const struct mg_connection* conn, void* cbdata);
};

#endif
```

### 3. WebManagerCivet.cpp (Key Implementation)
```cpp
#include "WebManagerCivet.h"
#include "EventLogger.h"
#include <Arduino.h>

// Client context for WebSocket connections
struct WsClientInfo {
    uint32_t id;
    uint32_t lastUpdate;
    bool canKeepUp;
    uint16_t updateRateHz;
};

WebManagerCivet::WebManagerCivet() : ctx(nullptr), serverPort(8081), isRunning(false) {
}

WebManagerCivet::~WebManagerCivet() {
    stop();
}

bool WebManagerCivet::begin(uint16_t port) {
    if (isRunning) return true;
    
    serverPort = port;
    
    // Initialize CivetWeb library
    mg_init_library(MG_FEATURES_WEBSOCKET);
    
    // Configure server options
    const char* options[] = {
        "listening_ports", String(port).c_str(),
        "num_threads", "2",
        "enable_keep_alive", "yes",
        "websocket_timeout_ms", "30000",
        "access_control_allow_origin", "*",
        NULL
    };
    
    // Setup callbacks
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    
    // Start server
    ctx = mg_start(&callbacks, this, options);
    if (!ctx) {
        LOG_ERROR(EventSource::SYSTEM, "Failed to start CivetWeb server");
        return false;
    }
    
    // Register handlers
    mg_set_request_handler(ctx, "/status", httpHandler, this);
    mg_set_request_handler(ctx, "/wstest", httpHandler, this);
    
    // Register WebSocket handler
    mg_set_websocket_handler(ctx, "/ws/telemetry",
                            wsConnectHandler,
                            wsReadyHandler,
                            wsDataHandler,
                            wsCloseHandler,
                            this);
    
    isRunning = true;
    LOG_INFO(EventSource::SYSTEM, "CivetWeb server started on port %d", port);
    return true;
}

void WebManagerCivet::stop() {
    if (ctx) {
        mg_stop(ctx);
        ctx = nullptr;
    }
    if (isRunning) {
        mg_exit_library();
        isRunning = false;
    }
}

void WebManagerCivet::broadcastTelemetry(const TelemetryPacket& packet) {
    if (!ctx) return;
    
    // Lock context for thread safety
    mg_lock_context(ctx);
    
    // Iterate through all connections
    struct mg_connection* conn;
    for (conn = mg_get_context_info(ctx, "first_conn"); 
         conn != NULL; 
         conn = mg_get_context_info(conn, "next_conn")) {
        
        // Check if this is a WebSocket connection
        if (mg_get_request_info(conn)->is_websocket) {
            WsClientInfo* client = (WsClientInfo*)mg_get_user_connection_data(conn);
            if (client) {
                uint32_t now = millis();
                uint32_t updateInterval = 1000 / client->updateRateHz;
                
                if (now - client->lastUpdate >= updateInterval) {
                    // Send binary telemetry packet
                    mg_websocket_write(conn, 
                                     MG_WEBSOCKET_OPCODE_BINARY,
                                     (const char*)&packet,
                                     sizeof(TelemetryPacket));
                    client->lastUpdate = now;
                }
            }
        }
    }
    
    mg_unlock_context(ctx);
}

// WebSocket callbacks
int WebManagerCivet::wsConnectHandler(const struct mg_connection* conn, void* cbdata) {
    // Allocate client context
    WsClientInfo* client = new WsClientInfo();
    client->id = millis(); // Simple ID
    client->lastUpdate = 0;
    client->canKeepUp = true;
    client->updateRateHz = 10; // Default 10Hz
    
    mg_set_user_connection_data(conn, client);
    
    LOG_INFO(EventSource::SYSTEM, "WebSocket client connected");
    return 0; // Accept connection
}

void WebManagerCivet::wsReadyHandler(struct mg_connection* conn, void* cbdata) {
    // Send initial configuration
    const char* config = "{\"type\":\"config\",\"rate\":10,\"version\":1}";
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, config, strlen(config));
}

int WebManagerCivet::wsDataHandler(struct mg_connection* conn, int opcode,
                                 char* data, size_t data_len, void* cbdata) {
    if (opcode == MG_WEBSOCKET_OPCODE_TEXT && data_len > 0) {
        // Handle rate change commands
        if (strncmp(data, "rate:", 5) == 0) {
            int rate = atoi(data + 5);
            if (rate >= 1 && rate <= 100) {
                WsClientInfo* client = (WsClientInfo*)mg_get_user_connection_data(conn);
                if (client) {
                    client->updateRateHz = rate;
                    
                    char response[64];
                    snprintf(response, sizeof(response), 
                           "{\"type\":\"ack\",\"rate\":%d}", rate);
                    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, 
                                     response, strlen(response));
                }
            }
        }
    }
    return 1; // Keep connection open
}

void WebManagerCivet::wsCloseHandler(const struct mg_connection* conn, void* cbdata) {
    WsClientInfo* client = (WsClientInfo*)mg_get_user_connection_data(conn);
    if (client) {
        delete client;
    }
    LOG_INFO(EventSource::SYSTEM, "WebSocket client disconnected");
}
```

## Integration Steps

### 1. Git Reset
```bash
# Save the knowledge docs first!
git add docs/WEBSOCKET_MIGRATION_LESSONS.md
git add docs/CIVETWEB_IMPLEMENTATION_GUIDE.md
git commit -m "Add WebSocket migration documentation"

# Reset other changes
git reset --hard HEAD
```

### 2. Copy CivetWeb Files
```bash
# Copy the actual CivetWeb source
cp test/civetweb/src/civetweb.c lib/civetweb/
cp test/civetweb/include/civetweb.h lib/civetweb/
```

### 3. Implement Clean Integration
- Follow the patterns in this guide
- Use CivetWeb's native API
- Test incrementally

## Testing Plan

### Phase 1: Basic HTTP
1. Implement simple status endpoint
2. Verify server starts and responds

### Phase 2: WebSocket Connection
1. Test single WebSocket connection
2. Verify handshake and initial message

### Phase 3: Telemetry Streaming
1. Send telemetry at 10Hz
2. Increase to 50Hz, then 100Hz
3. Test with multiple clients

### Phase 4: Stability
1. Run for extended periods
2. Test disconnect/reconnect cycles
3. Verify no memory leaks

## Benefits of This Approach

1. **Simplicity**: Using CivetWeb as intended
2. **Reliability**: Battle-tested library
3. **Performance**: Optimized C implementation
4. **Maintainability**: Less custom code
5. **Features**: Access to full CivetWeb capabilities

## Common Pitfalls to Avoid

1. Don't store mg_connection pointers
2. Don't parse WebSocket frames manually
3. Don't implement custom HTTP parsing
4. Always use mg_lock_context for thread safety
5. Free user data in close handler

This implementation will be much cleaner and more reliable than the custom wrapper approach.