// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleHTTPServer.cpp
// Lightweight HTTP server implementation using QNEthernet

#include "SimpleHTTPServer.h"
#include "EventLogger.h"

SimpleHTTPServer::SimpleHTTPServer() : 
    server(80),
    serverPort(80),
    running(false) {
}

SimpleHTTPServer::~SimpleHTTPServer() {
    stop();
}

bool SimpleHTTPServer::begin(uint16_t port) {
    serverPort = port;
    server = EthernetServer(port);
    server.begin();
    running = true;
    
    LOG_INFO(EventSource::NETWORK, "HTTP server started on port %d", port);
    return true;
}

void SimpleHTTPServer::stop() {
    if (running) {
        server.end();
        running = false;
        LOG_INFO(EventSource::NETWORK, "HTTP server stopped");
    }
}

void SimpleHTTPServer::handleClient() {
    if (!running) return;
    
    EthernetClient client = server.available();
    if (client) {
        String method, path, query;
        
        if (parseRequest(client, method, path, query)) {
            // Host-based virtual routing: requests to <shortname>.aog
            // are routed as /<shortname>/<path> to the module proxy.
            // This allows "http://sc.aog" to reach the Section Control
            // directly from the PC connected to the Teensy via LAN.
            // Exception: aio.aog, gps.aog, steer.aog serve the Teensy's
            // own pages (fall through to normal route matching).
            bool handledByHost = false;
            if (hostHeader.length() > 0) {
                // Check if host ends with ".local" or ".aog" (module shortname)
                int dotPos = hostHeader.indexOf(".local");
                if (dotPos < 0) dotPos = hostHeader.indexOf(".aog");
                if (dotPos > 0) {
                    String shortname = hostHeader.substring(0, dotPos);
                    // Determine if this hostname serves the Teensy's own pages.
                    // 'aio' is permanently fixed → Teensy home page.
                    // All other owned names (gps, steer, user aliases) come from
                    // ownedHostnames which is populated by SimpleWebManager at startup.
                    // 'wifi' is NOT treated as owned here: wifi.aog gets rewritten to
                    // /wifi/<path> so the /wifi/ route serves the ESP32 bridge page.
                    bool isOwned = (shortname == "aio");
                    if (!isOwned) {
                        for (const auto& h : ownedHostnames) {
                            if (h.equalsIgnoreCase(shortname)) { isOwned = true; break; }
                        }
                    }
                    if (!isOwned) {
                        // Host-based rewrite for module routing.
                        // Examples:
                        //   host=sc.local,   path=/           -> /sc/
                        //   host=sc.local,   path=/settings   -> /sc/settings
                        //   host=sc.local,   path=/sc/settings -> /sc/settings (already prefixed)
                        //   host=wifi.local, path=/wifi/settings -> /wifi/settings (already prefixed)
                        String modulePrefix = "/" + shortname;
                        String rewrittenPath;
                        if (path == modulePrefix || path.startsWith(modulePrefix + "/")) {
                            rewrittenPath = path;
                        } else if (path == "/") {
                            rewrittenPath = modulePrefix + "/";
                        } else {
                            rewrittenPath = modulePrefix + path;
                        }
                        // Try a registered route first (e.g. /wifi/ has its own handler)
                        Route* r = findRoute(rewrittenPath);
                        if (r) {
                            r->handler(client, method, query);
                        } else if (notFoundHandler) {
                            notFoundHandler(client, rewrittenPath, query);
                        }
                        handledByHost = true;
                    }
                }
            }

            if (!handledByHost) {
                Route* route = findRoute(path);
                if (route) {
                    route->handler(client, method, query);
                } else if (notFoundHandler) {
                    notFoundHandler(client, path, query);
                } else {
                    Serial.printf("404: %s\n", path.c_str());
                    handleNotFound(client);
                }
            }
        } else {
            Serial.println("HTTP: Parse failed");
        }
        
        // Close connection
        client.stop();
    }
}

bool SimpleHTTPServer::parseRequest(EthernetClient& client, String& method, String& path, String& query) {
    char line[256];
    
    // Read request line
    int len = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (len <= 0) return false;
    
    line[len] = '\0';
    
    // Parse method and path
    char methodBuf[16] = {0};
    char pathBuf[128] = {0};
    
    if (sscanf(line, "%15s %127s", methodBuf, pathBuf) != 2) {
        return false;
    }
    
    method = String(methodBuf);
    String fullPath = String(pathBuf);
    
    // Split path and query
    int queryIndex = fullPath.indexOf('?');
    if (queryIndex >= 0) {
        path = fullPath.substring(0, queryIndex);
        query = fullPath.substring(queryIndex + 1);
    } else {
        path = fullPath;
        query = "";
    }
    
    // Read headers - capture Host for virtual hosting
    hostHeader = "";
    while (client.available()) {
        len = client.readBytesUntil('\n', line, sizeof(line) - 1);
        if (len <= 1) break;  // Empty line marks end of headers
        line[len] = '\0';
        // Parse Host header
        if (strncasecmp(line, "Host:", 5) == 0) {
            char* h = line + 5;
            while (*h == ' ') h++;  // trim leading space
            // Remove trailing \r\n and port
            char* colon = strchr(h, ':');
            if (colon) *colon = '\0';
            char* cr = strchr(h, '\r');
            if (cr) *cr = '\0';
            hostHeader = String(h);
        }
    }
    
    return true;
}

void SimpleHTTPServer::on(const String& path, HTTPHandler handler) {
    Route route;
    route.path = path;
    route.handler = handler;
    routes.push_back(route);
}

void SimpleHTTPServer::addOwnedHostname(const String& name) {
    // Avoid duplicates
    for (const auto& h : ownedHostnames) {
        if (h.equalsIgnoreCase(name)) return;
    }
    ownedHostnames.push_back(name);
}

SimpleHTTPServer::Route* SimpleHTTPServer::findRoute(const String& path) {
    for (auto& route : routes) {
        if (route.path == path) {
            return &route;
        }
    }
    return nullptr;
}

void SimpleHTTPServer::handleNotFound(EthernetClient& client) {
    send(client, 404, "text/plain", "Not Found");
}

// Static helper methods

void SimpleHTTPServer::send(EthernetClient& client, int code, const String& contentType, const String& content) {
    String status;
    switch (code) {
        case 200: status = "OK"; break;
        case 301: status = "Moved Permanently"; break;
        case 302: status = "Found"; break;
        case 400: status = "Bad Request"; break;
        case 404: status = "Not Found"; break;
        case 500: status = "Internal Server Error"; break;
        case 503: status = "Service Unavailable"; break;
        default: status = "Unknown"; break;
    }
    
    // Debug - comment out for production
    // Serial.printf("HTTP Send: %d %s, estimated len=%d\n", code, status.c_str(), content.length());
    
    // Send with Content-Length so the browser knows exactly how much data to expect.
    // Without Content-Length the browser has to wait for TCP connection close to detect
    // the end of the response, which causes visible "fragmenting" for large pages.
    client.printf("HTTP/1.1 %d %s\r\n", code, status.c_str());
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.printf("Content-Length: %u\r\n", (unsigned int)content.length());
    client.print("Connection: close\r\n");
    client.print("\r\n");

    // Send content in chunks to handle TCP backpressure.
    // QNEthernet's write() returns 0 when the TX buffer is momentarily full,
    // NOT necessarily because the client disconnected. We must retry with a
    // short yield instead of aborting immediately.
    const size_t SEND_CHUNK = 1460;  // ~1 Ethernet MTU payload
    size_t sent = 0;
    const char* data = content.c_str();
    size_t total = content.length();
    uint32_t sendDeadline = millis() + 15000;  // max 15s total send time for large pages
    while (sent < total && millis() < sendDeadline) {
        size_t toSend = total - sent;
        if (toSend > SEND_CHUNK) toSend = SEND_CHUNK;
        size_t written = client.write(reinterpret_cast<const uint8_t*>(data + sent), toSend);
        if (written > 0) {
            sent += written;
        } else {
            // TX buffer full - yield briefly and retry
            // Do NOT break: a 0-return from QNEthernet write() is backpressure, not disconnect
            Ethernet.loop();  // process lwIP stack to drain TX buffer
            delayMicroseconds(100);
        }
    }
    client.flush();
}

void SimpleHTTPServer::sendP(EthernetClient& client, int code, const String& contentType, const char* content) {
    String status;
    switch (code) {
        case 200: status = "OK"; break;
        case 301: status = "Moved Permanently"; break;
        case 302: status = "Found"; break;
        case 400: status = "Bad Request"; break;
        case 404: status = "Not Found"; break;
        case 500: status = "Internal Server Error"; break;
        case 503: status = "Service Unavailable"; break;
        default: status = "Unknown"; break;
    }
    
    // Debug - comment out for production
    // Serial.printf("HTTP SendP: %d %s\n", code, status.c_str());
    
    // Send without Content-Length
    client.printf("HTTP/1.1 %d %s\r\n", code, status.c_str());
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    // Send PROGMEM content in chunks
    const size_t chunkSize = 256;  // Smaller chunks
    char buffer[chunkSize + 1];  // +1 for null terminator
    const char* ptr = content;
    size_t totalSent = 0;
    
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        size_t copied = 0;
        
        // Copy up to chunkSize bytes from PROGMEM
        for (size_t i = 0; i < chunkSize; i++) {
            char c = pgm_read_byte(ptr++);
            if (c == 0) {
                // End of string
                if (copied > 0) {
                    size_t sent = client.write(buffer, copied);
                    totalSent += sent;
                }
                // Serial.printf("HTTP SendP: Complete, sent %d bytes total\n", totalSent);
                client.flush();
                return;
            }
            buffer[i] = c;
            copied++;
        }
        
        // Send this chunk
        if (copied > 0) {
            size_t sent = 0;
            size_t toSend = copied;
            size_t offset = 0;
            
            // Send in smaller pieces if needed, waiting for client to be ready
            while (toSend > 0) {
                // Wait for client to be ready (up to 100ms)
                uint32_t waitStart = millis();
                while (!client.availableForWrite() && (millis() - waitStart < 100)) {
                    delay(1);
                }
                
                // Try to send what we can
                size_t canSend = client.availableForWrite();
                if (canSend > toSend) canSend = toSend;
                if (canSend > 64) canSend = 64;  // Limit chunk size to avoid buffer issues
                
                if (canSend > 0) {
                    size_t written = client.write(buffer + offset, canSend);
                    if (written > 0) {
                        sent += written;
                        offset += written;
                        toSend -= written;
                        totalSent += written;
                    } else {
                        // Write failed completely
                        Serial.printf("HTTP SendP: Write failed at %d bytes\n", totalSent);
                        return;
                    }
                } else {
                    // Client not ready after timeout
                    Serial.printf("HTTP SendP: Client not ready at %d bytes\n", totalSent);
                    return;
                }
                
                // Small delay to let network catch up
                if (totalSent % 512 == 0) {
                    delay(1);
                }
            }
        }
    }
}

void SimpleHTTPServer::sendJSON(EthernetClient& client, const String& json) {
    send(client, 200, "application/json", json);
}

void SimpleHTTPServer::redirect(EthernetClient& client, const String& location) {
    client.print("HTTP/1.1 302 Found\r\n");
    client.printf("Location: %s\r\n", location.c_str());
    client.print("Content-Length: 0\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    client.flush();
}