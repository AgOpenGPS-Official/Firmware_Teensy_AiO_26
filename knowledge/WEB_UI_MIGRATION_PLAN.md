# Web UI Migration Plan: Mongoose to AsyncWebServer

## Overview
This plan outlines the migration from Mongoose web framework to a basic web stack using QNEthernet, AsyncWebServer, and custom HTML/CSS/JavaScript.

## Required Libraries

### Core Networking
1. **QNEthernet** (already in project)
   - Ethernet driver for Teensy 4.1
   - Repository: https://github.com/ssilverman/QNEthernet

### Web Server
2. **ESPAsyncWebServer-Teensy41**
   - Async web server ported for Teensy 4.1
   - Repository: https://github.com/khoih-prog/AsyncWebServer_Teensy41
   - Dependencies: AsyncTCP_Teensy41

3. **AsyncTCP_Teensy41**
   - Async TCP library for Teensy 4.1
   - Repository: https://github.com/khoih-prog/AsyncTCP_Teensy41

### Optional but Recommended
4. **ArduinoJson** (already in project)
   - JSON serialization/deserialization
   - Version: 6.x or 7.x

5. **FlasherX**
   - OTA updates for Teensy
   - Repository: https://github.com/joepasquariello/FlasherX

## Migration Phases

### Phase 1: Setup and Basic Web Server (Checkpoint 1)
**Goal**: Replace Mongoose HTTP server with AsyncWebServer

1. **Create git branch**
   ```bash
   git checkout -b feature/async-web-ui
   ```

2. **Install libraries**
   - Add to platformio.ini:
   ```ini
   lib_deps = 
     khoih-prog/AsyncWebServer_Teensy41
     khoih-prog/AsyncTCP_Teensy41
     joepasquariello/FlasherX
   ```

3. **Create WebManager class**
   ```cpp
   // lib/aio_network/WebManager.h
   class WebManager {
   public:
     bool begin(uint16_t port = 80);
     void handleClient();
     void stop();
   private:
     AsyncWebServer* server;
   };
   ```

4. **Basic test endpoints**
   - GET / - returns "Hello World"
   - GET /api/status - returns JSON status

5. **Compile & Test Checkpoint 1**
   - Verify server starts
   - Test with browser/curl
   - Check memory usage
   - Verify no conflicts with existing code

### Phase 2: Static File Serving (Checkpoint 2)
**Goal**: Serve HTML/CSS/JS files from PROGMEM or SD card

1. **Create file structure**
   ```
   data/
   ├── index.html
   ├── style.css
   └── app.js
   ```

2. **Implement static file handler**
   - Serve from PROGMEM initially
   - Option for SD card later

3. **Create basic HTML template**
   ```html
   <!DOCTYPE html>
   <html>
   <head>
     <meta name="viewport" content="width=device-width, initial-scale=1">
     <title>AiO v26</title>
     <link rel="stylesheet" href="/style.css">
   </head>
   <body>
     <h1>System Status</h1>
     <div id="content"></div>
     <script src="/app.js"></script>
   </body>
   </html>
   ```

4. **Compile & Test Checkpoint 2**
   - Verify static files load
   - Check CSS applies correctly
   - Test JavaScript execution
   - Monitor response times

### Phase 3: REST API Implementation (Checkpoint 3)
**Goal**: Create REST endpoints for EventLogger

1. **Design API structure**
   ```
   GET  /api/eventlogger/config
   POST /api/eventlogger/config
   GET  /api/eventlogger/stats
   POST /api/eventlogger/reset
   ```

2. **Implement endpoints in WebManager**
   ```cpp
   void setupEventLoggerAPI() {
     server->on("/api/eventlogger/config", HTTP_GET, 
       [](AsyncWebServerRequest *request){
         // Return current config as JSON
       });
   }
   ```

3. **Add CORS headers for development**

4. **Create JavaScript API client**
   ```javascript
   class EventLoggerAPI {
     async getConfig() { }
     async saveConfig(config) { }
     async getStats() { }
   }
   ```

5. **Compile & Test Checkpoint 3**
   - Test each endpoint with curl
   - Verify JSON parsing
   - Check error handling
   - Test with EventLogger integration

### Phase 4: EventLogger Web UI (Checkpoint 4)
**Goal**: Create full-featured EventLogger configuration page

1. **HTML structure**
   - Configuration form
   - Statistics display
   - Action buttons

2. **Styling**
   - Responsive design
   - Mobile-friendly
   - Match existing AiO style

3. **JavaScript functionality**
   - Load/save configuration
   - Real-time validation
   - Error handling

4. **Compile & Test Checkpoint 4**
   - Test all UI functions
   - Verify data persistence
   - Test on mobile devices
   - Check memory/performance

### Phase 5: WebSocket Integration (Checkpoint 5)
**Goal**: Add real-time updates for logs and statistics

1. **Implement WebSocket handler**
   ```cpp
   AsyncWebSocket ws("/ws");
   void onWsEvent(AsyncWebSocket *server, 
                  AsyncWebSocketClient *client,
                  AwsEventType type, 
                  void *arg, 
                  uint8_t *data, 
                  size_t len);
   ```

2. **Create EventLogger hook**
   - Send log events to WebSocket clients
   - Throttle high-frequency updates

3. **JavaScript WebSocket client**
   ```javascript
   const ws = new WebSocket(`ws://${window.location.host}/ws`);
   ws.onmessage = (event) => {
     // Update UI with new data
   };
   ```

4. **Compile & Test Checkpoint 5**
   - Verify WebSocket connection
   - Test real-time updates
   - Check multiple clients
   - Monitor bandwidth usage

### Phase 6: OTA Implementation (Checkpoint 6)
**Goal**: Implement firmware update functionality

1. **Create OTA endpoint**
   - Use FlasherX for Teensy
   - Progress feedback
   - Error handling

2. **Build update UI**
   - File selection
   - Progress bar
   - Status messages

3. **Safety features**
   - Firmware validation
   - Version checking
   - Rollback mechanism

4. **Compile & Test Checkpoint 6**
   - Test with valid firmware
   - Test error conditions
   - Verify rollback works
   - Check update persistence

### Phase 7: Remove Mongoose (Checkpoint 7)
**Goal**: Clean removal of Mongoose dependencies

1. **Remove Mongoose files**
   - Delete lib/mongoose
   - Update platformio.ini
   - Remove mongoose includes

2. **Update initialization code**
   - Replace mongoose init with WebManager
   - Update network ready checks

3. **Final cleanup**
   - Remove unused code
   - Update documentation
   - Update CLAUDE.md

4. **Compile & Test Checkpoint 7**
   - Full system test
   - Memory usage comparison
   - Performance benchmarks
   - Regression testing

## Testing Strategy

### Unit Tests (at each checkpoint)
- API endpoint tests
- JSON serialization tests
- WebSocket message tests

### Integration Tests
- EventLogger integration
- Network stability
- Multi-client scenarios
- Memory leak detection

### User Acceptance Tests
- UI responsiveness
- Mobile compatibility
- Feature completeness
- Performance requirements

## Rollback Plan

If issues arise:
1. Each checkpoint is a stable state
2. Git commits at each checkpoint
3. Can revert to any checkpoint
4. Mongoose branch preserved

## Success Criteria

1. **Functionality**
   - All Mongoose features replaced
   - OTA updates working
   - Real-time updates via WebSocket

2. **Performance**
   - Smaller binary size
   - Faster page loads
   - Lower memory usage

3. **Maintainability**
   - Clear code structure
   - Well-documented
   - Easy to extend

## Timeline Estimate

- Phase 1: 2-3 hours
- Phase 2: 2-3 hours
- Phase 3: 3-4 hours
- Phase 4: 4-5 hours
- Phase 5: 3-4 hours
- Phase 6: 4-5 hours
- Phase 7: 2-3 hours

**Total: 20-27 hours of development**

## Next Steps

1. Review and approve plan
2. Create feature branch
3. Begin Phase 1 implementation
4. Regular checkpoint reviews

## Notes

- Each checkpoint includes compilation and testing
- No "big bang" changes - incremental progress
- Preserve working state at each checkpoint
- Document lessons learned