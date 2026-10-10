// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleWebSocket.cpp
// Lightweight WebSocket server implementation using QNEthernet

#include "SimpleWebSocket.h"
#include "EventLogger.h"
#include "base64_simple.h"
#include <algorithm>

// WebSocket magic string for handshake
const char WS_MAGIC_STRING[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Static member initialization
uint32_t WebSocketClient::nextClientId = 1;

// SHA1 implementation for WebSocket handshake
#include "sha1_simple.h"

//=============================================================================
// WebSocketClient Implementation
//=============================================================================

WebSocketClient::WebSocketClient(EthernetClient client) 
    : tcpClient(client), clientId(nextClientId++), handshakeComplete(false) {
    receiveBuffer.reserve(1024);
}

WebSocketClient::~WebSocketClient() {
    if (isConnected()) {
        close();
    }
}

bool WebSocketClient::isConnected() const {
    return tcpClient.connected() && handshakeComplete;
}

bool WebSocketClient::poll() {
    if (!tcpClient.connected()) {
        return false;
    }
    
    // Perform handshake if not done
    if (!handshakeComplete) {
        return performHandshake();
    }
    
    // Drain TCP into the receive buffer, bounded so a peer cannot grow it without limit
    const size_t maxBuffered = MAX_PAYLOAD + 14;  // payload + max header + mask
    while (tcpClient.available() && receiveBuffer.size() < maxBuffered) {
        uint8_t tmp[128];
        size_t room = maxBuffered - receiveBuffer.size();
        int n = tcpClient.read(tmp, min(sizeof(tmp), room));
        if (n <= 0) break;
        receiveBuffer.insert(receiveBuffer.end(), tmp, tmp + n);
    }

    // Parse every complete frame; partial frames wait for the next poll
    while (!receiveBuffer.empty()) {
        WSFrameHeader header;
        std::vector<uint8_t> payload;

        FrameResult result = readFrame(header, payload);
        if (result == FrameResult::NEED_MORE) {
            break;
        }
        if (result == FrameResult::OK) {
            processFrame(header, payload);
            continue;
        }
        close(result == FrameResult::TOO_LARGE ? 1009 : 1002,
              result == FrameResult::TOO_LARGE ? "Message too big" : "Protocol error");
        return false;
    }

    return true;
}

bool WebSocketClient::performHandshake() {
    LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d performing handshake", clientId);
    
    // Read HTTP request
    String line;
    String wsKey;
    bool upgradeFound = false;
    bool connectionFound = false;
    
    // Wait a bit for data to arrive
    uint32_t start = millis();
    while (!tcpClient.available() && millis() - start < 100) {
        delay(1);
    }
    
    if (!tcpClient.available()) {
        LOG_WARNING(EventSource::NETWORK, "WebSocket client %d no data for handshake", clientId);
        return false;
    }
    
    while (tcpClient.available()) {
        char c = tcpClient.read();
        if (c == '\r') continue;
        
        if (c == '\n') {
            if (line.length() == 0) {
                // Empty line marks end of headers
                break;
            }
            
            // Parse header
            if (line.startsWith("Sec-WebSocket-Key: ")) {
                wsKey = line.substring(19);
                wsKey.trim();
            } else if (line.indexOf("Upgrade: websocket") >= 0) {
                upgradeFound = true;
            } else if (line.indexOf("Connection: Upgrade") >= 0) {
                connectionFound = true;
            }
            
            line = "";
        } else {
            line += c;
        }
    }
    
    // Validate handshake
    if (!upgradeFound || !connectionFound || wsKey.length() == 0) {
        tcpClient.print("HTTP/1.1 400 Bad Request\r\n\r\n");
        return false;
    }
    
    // Generate accept key
    String acceptKey = generateAcceptKey(wsKey);
    
    // Send handshake response
    tcpClient.print("HTTP/1.1 101 Switching Protocols\r\n");
    tcpClient.print("Upgrade: websocket\r\n");
    tcpClient.print("Connection: Upgrade\r\n");
    tcpClient.print("Sec-WebSocket-Accept: ");
    tcpClient.print(acceptKey);
    tcpClient.print("\r\n\r\n");
    tcpClient.flush();
    
    handshakeComplete = true;
    LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d handshake complete", clientId);
    
    return true;
}

String WebSocketClient::generateAcceptKey(const String& key) {
    // Concatenate key with magic string
    String concat = key + WS_MAGIC_STRING;
    
    // Calculate SHA1
    uint8_t hash[SHA1_HASH_SIZE];
    sha1((const uint8_t*)concat.c_str(), concat.length(), hash);
    
    // Base64 encode
    String encoded = base64::encode(hash, SHA1_HASH_SIZE);
    return encoded;
}

WebSocketClient::FrameResult WebSocketClient::readFrame(WSFrameHeader& header, std::vector<uint8_t>& payload) {
    const uint8_t* buf = receiveBuffer.data();
    size_t avail = receiveBuffer.size();
    if (avail < 2) return FrameResult::NEED_MORE;

    uint8_t byte1 = buf[0];
    uint8_t byte2 = buf[1];
    size_t pos = 2;

    header.fin = (byte1 & 0x80) != 0;
    header.rsv1 = (byte1 & 0x40) != 0;
    header.rsv2 = (byte1 & 0x20) != 0;
    header.rsv3 = (byte1 & 0x10) != 0;
    header.opcode = static_cast<WSOpcode>(byte1 & 0x0F);
    header.masked = (byte2 & 0x80) != 0;

    uint64_t len = byte2 & 0x7F;
    if (len == 126) {
        if (avail < pos + 2) return FrameResult::NEED_MORE;
        len = ((uint64_t)buf[pos] << 8) | buf[pos + 1];
        pos += 2;
    } else if (len == 127) {
        if (avail < pos + 8) return FrameResult::NEED_MORE;
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | buf[pos + i];
        pos += 8;
    }

    if (len > MAX_PAYLOAD) return FrameResult::TOO_LARGE;
    header.payloadLength = len;

    if (header.masked) {
        if (avail < pos + 4) return FrameResult::NEED_MORE;
        memcpy(header.maskKey, buf + pos, 4);
        pos += 4;
    }

    if (avail < pos + len) return FrameResult::NEED_MORE;

    payload.assign(buf + pos, buf + pos + len);
    if (header.masked) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= header.maskKey[i % 4];
        }
    }

    receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + pos + len);
    return FrameResult::OK;
}

bool WebSocketClient::sendFrame(WSOpcode opcode, const uint8_t* data, size_t length) {
    if (!isConnected()) return false;
    
    // Build frame header
    uint8_t header[10];
    size_t headerLen = 2;
    
    // FIN = 1, RSV = 0, Opcode
    header[0] = 0x80 | static_cast<uint8_t>(opcode);
    
    // Mask = 0 (server doesn't mask), payload length
    if (length < 126) {
        header[1] = length;
    } else if (length < 65536) {
        header[1] = 126;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        headerLen = 4;
    } else {
        // For larger payloads, we'd need 64-bit length
        // For telemetry, we shouldn't need this
        return false;
    }
    
    // Send header
    tcpClient.write(header, headerLen);
    
    // Send payload
    if (length > 0 && data != nullptr) {
        tcpClient.write(data, length);
    }
    
    return true;
}

bool WebSocketClient::sendBinary(const uint8_t* data, size_t length) {
    return sendFrame(WSOpcode::BINARY, data, length);
}

bool WebSocketClient::sendText(const String& text) {
    return sendFrame(WSOpcode::TEXT, (const uint8_t*)text.c_str(), text.length());
}

bool WebSocketClient::sendPing() {
    return sendFrame(WSOpcode::PING, nullptr, 0);
}

void WebSocketClient::close(uint16_t code, const String& reason) {
    if (tcpClient.connected()) {
        // Send close frame
        uint8_t payload[125];  // Max reason length
        payload[0] = (code >> 8) & 0xFF;
        payload[1] = code & 0xFF;
        
        size_t reasonLen = min(reason.length(), (size_t)123);
        if (reasonLen > 0) {
            memcpy(&payload[2], reason.c_str(), reasonLen);
        }
        
        sendFrame(WSOpcode::CLOSE, payload, 2 + reasonLen);
        
        // Close TCP connection
        tcpClient.stop();
    }
    
    handshakeComplete = false;
    
    if (closeCallback) {
        closeCallback();
    }
}

void WebSocketClient::processFrame(const WSFrameHeader& header, const std::vector<uint8_t>& payload) {
    switch (header.opcode) {
        case WSOpcode::BINARY:
        case WSOpcode::TEXT:
            if (messageCallback) {
                messageCallback(payload.data(), payload.size(), header.opcode == WSOpcode::BINARY);
            }
            break;
            
        case WSOpcode::PING:
            // Respond with pong
            sendFrame(WSOpcode::PONG, payload.data(), payload.size());
            break;
            
        case WSOpcode::CLOSE:
            // Close connection
            close();
            break;
            
        default:
            break;
    }
}

//=============================================================================
// SimpleWebSocketServer Implementation
//=============================================================================

SimpleWebSocketServer::SimpleWebSocketServer() 
    : server(80), maxClients(4), running(false) {
}

SimpleWebSocketServer::~SimpleWebSocketServer() {
    stop();
}

bool SimpleWebSocketServer::begin(uint16_t port) {
    server = EthernetServer(port);
    server.begin();
    running = true;
    
    LOG_INFO(EventSource::NETWORK, "WebSocket server started on port %d", port);
    return true;
}

void SimpleWebSocketServer::stop() {
    if (running) {
        // Close all client connections
        clients.clear();
        
        // Stop server
        server.end();
        running = false;
        
        LOG_INFO(EventSource::NETWORK, "WebSocket server stopped");
    }
}

void SimpleWebSocketServer::handleClients() {
    if (!running) return;
    
    // Accept new clients
    acceptNewClients();
    
    // Poll existing clients
    for (auto& client : clients) {
        if (client && !client->poll()) {
            // Client disconnected
            client.reset();
        }
    }
    
    // Remove null clients
    removeDisconnectedClients();
}

void SimpleWebSocketServer::acceptNewClients() {
    EthernetClient newClient = server.available();
    
    if (newClient) {
        LOG_DEBUG(EventSource::NETWORK, "New TCP connection on WebSocket port");
        
        // Check if we have room
        if (clients.size() >= maxClients) {
            // Reject connection
            newClient.print("HTTP/1.1 503 Service Unavailable\r\n\r\n");
            newClient.stop();
            LOG_WARNING(EventSource::NETWORK, "WebSocket connection rejected - max clients reached");
            return;
        }
        
        // Create WebSocket client
        auto wsClient = std::make_unique<WebSocketClient>(newClient);
        uint32_t clientId = wsClient->getClientId();
        clients.push_back(std::move(wsClient));
        
        LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d created, waiting for handshake", clientId);
    }
}

void SimpleWebSocketServer::removeDisconnectedClients() {
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
                      [](const std::unique_ptr<WebSocketClient>& client) {
                          return !client || !client->isConnected();
                      }),
        clients.end()
    );
}

size_t SimpleWebSocketServer::getClientCount() const {
    size_t count = 0;
    for (const auto& client : clients) {
        if (client && client->isConnected()) {
            count++;
        }
    }
    return count;
}

void SimpleWebSocketServer::broadcastBinary(const uint8_t* data, size_t length) {
    uint32_t start = micros();

    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendBinary(data, length);
        }
    }

    uint32_t elapsed = micros() - start;
    perfSendTime += elapsed;
    perfSendCount++;
}

void SimpleWebSocketServer::logPeriodicStatus() {
    if (perfSendCount > 0) {
        float avgSendTime = perfSendTime / (float)perfSendCount;
        LOG_INFO(EventSource::NETWORK, "WebSocket: %.1f us/send, %d clients",
                 avgSendTime, getClientCount());
    } else {
        LOG_INFO(EventSource::NETWORK, "WebSocket: idle, %d clients", getClientCount());
    }
    perfSendCount = 0;
    perfSendTime = 0;
}

void SimpleWebSocketServer::broadcastText(const String& text) {
    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendText(text);
        }
    }
}

void SimpleWebSocketServer::broadcast(const char* text, size_t length) {
    String str;
    str.reserve(length);
    for (size_t i = 0; i < length; i++) {
        str += text[i];
    }

    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendText(str);
        }
    }
}

void SimpleWebSocketServer::sendToClient(size_t index, const char* text, size_t length) {
    if (index < clients.size() && clients[index] && clients[index]->isConnected()) {
        String str;
        str.reserve(length);
        for (size_t i = 0; i < length; i++) {
            str += text[i];
        }
        clients[index]->sendText(str);
    }
}