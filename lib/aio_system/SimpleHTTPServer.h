// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleHTTPServer.h
// Lightweight HTTP server implementation using QNEthernet
// Optimized for serving PROGMEM content efficiently

#ifndef SIMPLE_HTTP_SERVER_H
#define SIMPLE_HTTP_SERVER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include <functional>
#include <vector>

using namespace qindesign::network;

// Route handler function type - simplified for efficiency
using HTTPHandler = std::function<void(EthernetClient&, const String&, const String&)>;

// Not-found handler: receives client, full path, query string
using HTTPNotFoundHandler = std::function<void(EthernetClient&, const String&, const String&)>;

// Simple HTTP server optimized for PROGMEM content
class SimpleHTTPServer {
public:
    SimpleHTTPServer();
    ~SimpleHTTPServer();
    
    // Server control
    bool begin(uint16_t port = 80);
    void stop();
    void handleClient();
    
    // Route registration - only GET and POST supported
    void on(const String& path, HTTPHandler handler);

    // Optional catch-all handler for unmatched routes (module proxy etc.)
    void onNotFound(HTTPNotFoundHandler handler) { notFoundHandler = handler; }

    // Register additional hostnames (without ".aog") that resolve to the Teensy's own interface.
    // By default "aio", "gps", "steer", "wifi" are owned. Call this to add user-configured aliases
    // so that e.g. "board.aog" serves the Teensy's own pages instead of the module proxy.
    void addOwnedHostname(const String& name);
    
    // Server info
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }
    
    // Helper methods for responses
    static void send(EthernetClient& client, int code, const String& contentType, const String& content);
    static void sendP(EthernetClient& client, int code, const String& contentType, const char* content);
    static void sendJSON(EthernetClient& client, const String& json);
    static void redirect(EthernetClient& client, const String& location);
    
private:
    struct Route {
        String path;
        HTTPHandler handler;
    };
    
    EthernetServer server;
    std::vector<Route> routes;
    std::vector<String> ownedHostnames;  // Extra shortnames that serve the Teensy's own pages
    uint16_t serverPort;
    bool running;
    HTTPNotFoundHandler notFoundHandler;  // optional catch-all for module proxy
    String hostHeader;  // Host header from current request (for virtual hosting)
    
    // Request parsing
    bool parseRequest(EthernetClient& client, String& method, String& path, String& query);
    
    // Route matching
    Route* findRoute(const String& path);
    
    // Default 404 handler
    void handleNotFound(EthernetClient& client);
};

#endif // SIMPLE_HTTP_SERVER_H