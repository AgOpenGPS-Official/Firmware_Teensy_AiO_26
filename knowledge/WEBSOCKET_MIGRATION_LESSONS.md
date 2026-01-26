# WebSocket Migration: Lessons Learned

## Overview
This document captures lessons learned from the initial attempt to migrate from AsyncWebServer to CivetWeb for WebSocket support in the AiO v26 project.

## Background
- **Problem**: AsyncWebServer's SSE functionality is broken, forcing polling which cannot support 100Hz telemetry updates
- **Goal**: Implement WebSocket support for high-frequency telemetry streaming
- **Initial Approach**: Created a custom wrapper around CivetWeb functionality

## What Went Wrong

### 1. Reinventing the Wheel
We created `CivetWebTeensy` as a custom wrapper, reimplementing:
- HTTP request parsing
- WebSocket frame handling
- Connection management
- SHA1/Base64 encoding for handshakes

**Problem**: CivetWeb already provides all of this functionality through its native API.

### 2. Memory Management Issues
- Stored pointers to temporary `mg_connection` objects
- Created complex connection tracking systems
- Experienced crashes due to invalid pointer access

**Problem**: CivetWeb handles connection lifecycle internally; we shouldn't store connection pointers.

### 3. Unnecessary Abstraction
- Created our own connection structures
- Built custom WebSocket frame parsers
- Implemented our own HTTP response handling

**Problem**: This duplicated CivetWeb's functionality and introduced bugs.

## The Right Approach

### 1. Use CivetWeb Directly
```c
// Initialize library
mg_init_library(MG_FEATURES_WEBSOCKET);

// Start server
struct mg_context *ctx = mg_start(&callbacks, NULL, options);

// Set WebSocket handler
mg_set_websocket_handler(ctx, "/ws/telemetry", 
                         ws_connect_handler,
                         ws_ready_handler,
                         ws_data_handler,
                         ws_close_handler,
                         NULL);
```

### 2. Proper User Data Management
```c
// In connect handler
struct ClientInfo *client = calloc(1, sizeof(struct ClientInfo));
mg_set_user_connection_data(conn, client);

// In other handlers
struct ClientInfo *client = mg_get_user_connection_data(conn);
```

### 3. Let CivetWeb Handle Protocol Details
- No manual WebSocket frame parsing
- No custom SHA1/Base64 implementation
- No connection list management

## Implementation Plan for Fresh Start

### Phase 1: Setup
1. Copy CivetWeb source files (civetweb.c, civetweb.h) to lib/civetweb
2. Configure compile flags to disable unneeded features:
   ```c
   #define NO_SSL
   #define NO_CGI
   #define NO_LUA
   #define USE_WEBSOCKET
   ```

### Phase 2: Basic Integration
1. Create simple WebManagerCivet that:
   - Initializes CivetWeb with `mg_init_library()`
   - Starts server with `mg_start()`
   - Registers HTTP handlers with `mg_set_request_handler()`
   - Registers WebSocket handlers with `mg_set_websocket_handler()`

### Phase 3: WebSocket Implementation
1. Implement proper CivetWeb callbacks:
   - `ws_connect_handler` - Accept/reject connections
   - `ws_ready_handler` - Client ready to receive data
   - `ws_data_handler` - Process incoming messages
   - `ws_close_handler` - Cleanup on disconnect

2. Use CivetWeb's thread-safe broadcast:
   ```c
   // CivetWeb provides connection iteration
   mg_lock_context(ctx);
   for (conn = mg_get_first_connection(ctx); conn; conn = mg_get_next_connection(conn)) {
       if (is_websocket_connection(conn)) {
           mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_BINARY, data, len);
       }
   }
   mg_unlock_context(ctx);
   ```

### Phase 4: Testing
1. Test with single connection
2. Test with multiple connections
3. Test at various update rates (10Hz, 50Hz, 100Hz)
4. Verify stability over extended periods

## Key Takeaways

1. **Don't reimplement existing functionality** - CivetWeb is designed for embedded systems
2. **Trust the library** - CivetWeb handles all protocol details correctly
3. **Keep it simple** - Use the provided API as intended
4. **Read the examples** - The ws_server.c example shows the correct pattern

## Code Size Considerations

CivetWeb can be configured for minimal size:
- Disable SSL/TLS support
- Disable CGI support  
- Disable Lua scripting
- Disable IPv6 if not needed
- Enable only required features

## Memory Usage

For Teensy 4.1:
- CivetWeb uses configurable thread pool
- Each connection has minimal overhead
- Binary WebSocket frames are efficient
- No unnecessary buffering

## Performance Expectations

With proper CivetWeb usage:
- 100Hz telemetry updates achievable
- Multiple simultaneous connections supported
- Low latency binary protocol
- Efficient memory usage

## Next Steps

1. Roll back current changes with git
2. Copy real CivetWeb source to project
3. Implement clean integration following CivetWeb examples
4. Test thoroughly before committing

This approach will result in cleaner, more maintainable, and more reliable code.