// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// GVRETTCPServer.cpp
// GVRET (GVRET/Automotive) protocol TCP server implementation

#include "GVRETTCPServer.h"
#include "CANGlobals.h"
#include "EventLogger.h"

// Device info string for GVRET
const char GVRET_DEVICE_INFO[] = "Teensy41 AiO v26 + AgOpenGPS\r\n";

GVRETTCPServer::GVRETTCPServer()
    : _enabled(false), _baseMicros(0) {
    memset(&_stats, 0, sizeof(_stats));
}

GVRETTCPServer::~GVRETTCPServer() {
    stop();
}

bool GVRETTCPServer::begin() {
    // Initialize servers on ports 2201, 2202, 2203
    _servers[0] = EthernetServer(2201);  // CAN1
    _servers[1] = EthernetServer(2202);  // CAN2
    _servers[2] = EthernetServer(2203);  // CAN3

    for (uint8_t i = 0; i < 3; i++) {
        _servers[i].begin();
    }

    _baseMicros = micros();
    _enabled = true;

    LOG_INFO(EventSource::NETWORK, "GVRET TCP Server started on ports 2201-2203");
    return true;
}

void GVRETTCPServer::stop() {
    if (_enabled) {
        for (uint8_t i = 0; i < 3; i++) {
            _servers[i].end();
            _clients[i].clear();
        }
        _enabled = false;
        LOG_INFO(EventSource::NETWORK, "GVRET TCP Server stopped");
    }
}

void GVRETTCPServer::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        // Disconnect all clients
        for (uint8_t i = 0; i < 3; i++) {
            _clients[i].clear();
        }
    }
}

bool GVRETTCPServer::isEnabled() const {
    return _enabled;
}

size_t GVRETTCPServer::getClientCount(uint8_t busNum) const {
    if (busNum >= 3) return 0;
    size_t count = 0;
    for (const auto& client : _clients[busNum]) {
        if (client && client->client.connected()) {
            count++;
        }
    }
    return count;
}

void GVRETTCPServer::resetStats() {
    memset(&_stats, 0, sizeof(_stats));
}

void GVRETTCPServer::handleClients() {
    if (!_enabled) return;

    // Poll CAN buses for new frames
    pollCANBuses();

    // Accept new clients and process existing ones
    for (uint8_t busNum = 0; busNum < 3; busNum++) {
        acceptNewClients(busNum);
        removeDisconnectedClients(busNum);

        // Process client data
        for (auto& client : _clients[busNum]) {
            if (client && client->client.connected()) {
                processClientData(client.get(), busNum);
            }
        }
    }
}

void GVRETTCPServer::pollCANBuses() {
    CAN_message_t msg;

    // Poll CAN1
    while (globalCAN1.read(msg)) {
        _stats.framesReceived[0]++;
        sendFrameToClients(0, msg);
    }

    // Poll CAN2
    while (globalCAN2.read(msg)) {
        _stats.framesReceived[1]++;
        sendFrameToClients(1, msg);
    }

    // Poll CAN3
    while (globalCAN3.read(msg)) {
        _stats.framesReceived[2]++;
        sendFrameToClients(2, msg);
    }
}

void GVRETTCPServer::sendFrameToClients(uint8_t busNum, const CAN_message_t& msg) {
    if (busNum >= 3) return;

    // Check if we have any connected clients for this bus
    if (_clients[busNum].empty()) return;

    // Build GVRET frame
    // Format: timestamp(4) + canId(4) + flags(1) + data(8) + checksum(1) = 18 bytes max
    uint8_t frame[18];
    uint32_t timestamp = getMicroseconds();
    uint32_t canId = msg.id;

    // Timestamp (little-endian)
    frame[0] = timestamp & 0xFF;
    frame[1] = (timestamp >> 8) & 0xFF;
    frame[2] = (timestamp >> 16) & 0xFF;
    frame[3] = (timestamp >> 24) & 0xFF;

    // CAN ID (little-endian)
    frame[4] = canId & 0xFF;
    frame[5] = (canId >> 8) & 0xFF;
    frame[6] = (canId >> 16) & 0xFF;
    frame[7] = (canId >> 24) & 0xFF;

    // Flags: bit 7 = extended, bits 0-3 = data length
    uint8_t flags = msg.len & 0x0F;
    if (msg.flags.extended) {
        flags |= 0x80;
    }
    frame[8] = flags;

    // Data bytes
    uint8_t dataLen = msg.len;
    if (dataLen > 8) dataLen = 8;
    memcpy(&frame[9], msg.buf, dataLen);

    // Padding to 16 bytes (9 + data + padding)
    uint8_t frameLen = 9 + dataLen;
    while (frameLen < 17) {
        frame[frameLen++] = 0;
    }

    // Checksum
    frame[frameLen] = calculateChecksum(frame, frameLen);
    frameLen++;

    // Send to all connected clients on this bus
    for (auto& client : _clients[busNum]) {
        if (client && client->client.connected() && client->binaryMode) {
            size_t sent = client->client.write(frame, frameLen);
            _stats.bytesSent[busNum] += sent;
        }
    }
}

void GVRETTCPServer::acceptNewClients(uint8_t busNum) {
    if (busNum >= 3) return;

    EthernetClient newClient = _servers[busNum].available();

    if (newClient) {
        // Check if we have room (max 4 clients per bus)
        if (_clients[busNum].size() >= 4) {
            // Reject connection
            newClient.stop();
            LOG_WARNING(EventSource::NETWORK, "GVRET port 220%d: Connection rejected - max clients reached", busNum + 1);
            return;
        }

        // Create new client
        auto client = std::make_unique<GVRETClient>();
        client->client = newClient;
        client->busNum = busNum;
        client->lastActivity = millis();
        client->rxState = GVRETClient::RXState::WAIT_PREFIX;
        client->rxIndex = 0;
        client->binaryMode = false;

        _clients[busNum].push_back(std::move(client));
        _stats.connections[busNum]++;

        LOG_INFO(EventSource::NETWORK, "GVRET port 220%d: New client connected", busNum + 1);
    }
}

void GVRETTCPServer::removeDisconnectedClients(uint8_t busNum) {
    if (busNum >= 3) return;

    _clients[busNum].erase(
        std::remove_if(_clients[busNum].begin(), _clients[busNum].end(),
                      [](const std::unique_ptr<GVRETClient>& client) {
                          return !client || !client->client.connected();
                      }),
        _clients[busNum].end()
    );
}

void GVRETTCPServer::processClientData(GVRETClient* client, uint8_t busNum) {
    if (!client || !client->client.connected()) return;

    // Read available data
    while (client->client.available()) {
        uint8_t byte = client->client.read();
        _stats.bytesReceived[busNum]++;
        client->lastActivity = millis();

        switch (client->rxState) {
            case GVRETClient::RXState::WAIT_PREFIX:
                // Looking for 0xF1 0xE7 prefix
                if (byte == 0xF1 && client->rxIndex == 0) {
                    client->rxBuffer[client->rxIndex++] = byte;
                } else if (byte == 0xE7 && client->rxIndex == 1) {
                    client->rxBuffer[client->rxIndex++] = byte;
                    client->rxState = GVRETClient::RXState::WAIT_COMMAND;
                    client->rxIndex = 0;
                } else {
                    // Invalid prefix, reset
                    client->rxIndex = 0;
                }
                break;

            case GVRETClient::RXState::WAIT_COMMAND:
                client->rxBuffer[client->rxIndex++] = byte;
                client->rxState = GVRETClient::RXState::WAIT_DATA;
                client->expectedBytes = getExpectedDataLength(byte);
                break;

            case GVRETClient::RXState::WAIT_DATA:
                client->rxBuffer[client->rxIndex++] = byte;

                if (client->rxIndex >= client->expectedBytes) {
                    // Process complete command
                    processCommand(client, busNum);

                    // Reset for next command
                    client->rxIndex = 0;
                    client->rxState = GVRETClient::RXState::WAIT_PREFIX;
                }
                break;
        }

        // Prevent buffer overflow
        if (client->rxIndex >= sizeof(client->rxBuffer)) {
            client->rxIndex = 0;
            client->rxState = GVRETClient::RXState::WAIT_PREFIX;
            _stats.errors++;
        }
    }
}

uint8_t GVRETTCPServer::getExpectedDataLength(uint8_t command) {
    switch (command) {
        case GVRETCommands::BINARY_MODE:
            return 0;  // No additional data
        case GVRETCommands::SEND_CAN_FRAME:
            return 13; // ID(4) + flags(1) + data(8)
        case GVRETCommands::TIME_SYNC:
            return 4;  // Timestamp(4)
        case GVRETCommands::GET_CANBUS_STATS:
            return 0;
        case GVRETCommands::GET_DEVICE_INFO:
            return 0;
        case GVRETCommands::SETUP_CAN_BUS:
            return 2;  // Bus(1) + speed(1)
        case GVRETCommands::COMM_VALIDATION:
            return 1;  // Value to echo
        default:
            return 0;  // Unknown command, minimal data
    }
}

void GVRETTCPServer::processCommand(GVRETClient* client, uint8_t busNum) {
    uint8_t command = client->rxBuffer[0];

    switch (command) {
        case GVRETCommands::BINARY_MODE:
            handleBinaryMode(client);
            break;

        case GVRETCommands::SEND_CAN_FRAME:
            handleSendCANFrame(client, busNum);
            break;

        case GVRETCommands::TIME_SYNC:
            handleTimeSync(client);
            break;

        case GVRETCommands::GET_CANBUS_STATS:
            handleGetCANBusStats(client, busNum);
            break;

        case GVRETCommands::GET_DEVICE_INFO:
            handleGetDeviceInfo(client);
            break;

        case GVRETCommands::SETUP_CAN_BUS:
            handleSetupCanBus(client);
            break;

        case GVRETCommands::COMM_VALIDATION:
            handleCommValidation(client);
            break;

        default:
            // Unknown command - send error response
            _stats.errors++;
            break;
    }
}

void GVRETTCPServer::handleBinaryMode(GVRETClient* client) {
    client->binaryMode = true;

    // Send acknowledgment with device info
    uint8_t response[] = {
        0xF1, 0xE7,  // Prefix
        GVRETCommands::GET_DEVICE_INFO,
        // Device info follows
    };
    sendResponse(client, response, sizeof(response));

    LOG_DEBUG(EventSource::NETWORK, "GVRET client switched to binary mode");
}

void GVRETTCPServer::handleSendCANFrame(GVRETClient* client, uint8_t busNum) {
    // Parse CAN frame from buffer
    // Buffer layout: command(1) + id(4) + flags(1) + data(8)
    uint32_t canId = client->rxBuffer[1] |
                     (client->rxBuffer[2] << 8) |
                     (client->rxBuffer[3] << 16) |
                     (client->rxBuffer[4] << 24);
    uint8_t flags = client->rxBuffer[5];
    uint8_t dataLen = flags & 0x0F;
    bool extended = (flags & 0x80) != 0;

    // Build and send CAN message
    CAN_message_t msg;
    msg.id = canId;
    msg.flags.extended = extended;
    msg.len = (dataLen > 8) ? 8 : dataLen;
    memcpy(msg.buf, &client->rxBuffer[6], msg.len);

    bool sent = false;

    // Send to appropriate CAN bus (use template-specific write)
    if (busNum == 0) {
        sent = globalCAN1.write(msg);
    } else if (busNum == 1) {
        sent = globalCAN2.write(msg);
    } else if (busNum == 2) {
        sent = globalCAN3.write(msg);
    }

    if (sent) {
        _stats.framesSent[busNum]++;
        LOG_DEBUG(EventSource::CAN, "GVRET: Sent CAN frame 0x%08X on bus %d", canId, busNum + 1);
    } else {
        _stats.errors++;
        LOG_WARNING(EventSource::CAN, "GVRET: Failed to send CAN frame on bus %d", busNum + 1);
    }
}

void GVRETTCPServer::handleTimeSync(GVRETClient* client) {
    // Buffer layout: command(1) + timestamp(4)
    uint32_t timestamp = client->rxBuffer[1] |
                        (client->rxBuffer[2] << 8) |
                        (client->rxBuffer[3] << 16) |
                        (client->rxBuffer[4] << 24);

    // Update base timestamp
    _baseMicros = timestamp;

    LOG_DEBUG(EventSource::NETWORK, "GVRET: Time sync received");
}

void GVRETTCPServer::handleGetCANBusStats(GVRETClient* client, uint8_t busNum) {
    // Response: bus(1) + rxCount(4) + txCount(4) + rxError(1) + txError(1) + overflow(1)
    uint8_t response[12];
    response[0] = busNum;

    uint32_t rxCount = _stats.framesReceived[busNum];
    uint32_t txCount = _stats.framesSent[busNum];

    response[1] = rxCount & 0xFF;
    response[2] = (rxCount >> 8) & 0xFF;
    response[3] = (rxCount >> 16) & 0xFF;
    response[4] = (rxCount >> 24) & 0xFF;

    response[5] = txCount & 0xFF;
    response[6] = (txCount >> 8) & 0xFF;
    response[7] = (txCount >> 16) & 0xFF;
    response[8] = (txCount >> 24) & 0xFF;

    // Error counts (not implemented for FlexCAN_T4)
    response[9] = 0;  // RX error
    response[10] = 0; // TX error
    response[11] = 0; // Overflow

    sendResponse(client, response, sizeof(response));
}

void GVRETTCPServer::handleGetDeviceInfo(GVRETClient* client) {
    // Send device info string
    sendResponse(client, (const uint8_t*)GVRET_DEVICE_INFO, strlen(GVRET_DEVICE_INFO));
}

void GVRETTCPServer::handleSetupCanBus(GVRETClient* client) {
    // Buffer layout: command(1) + bus(1) + speed(1)
    // Speed: 0=125k, 1=250k, 2=500k, 3=1M, 4=33.3k (SWS)
    uint8_t busNum = client->rxBuffer[1];
    uint8_t speedCode = client->rxBuffer[2];

    // Note: Changing CAN bus speed requires reinitialization
    // For now, just log the request (not implemented)
    LOG_INFO(EventSource::CAN, "GVRET: Setup CAN bus %d speed code %d (not implemented)", busNum, speedCode);

    // Send acknowledgment
    uint8_t response[] = {0xF1, 0xE7, GVRETCommands::SETUP_CAN_BUS, 1}; // 1 = success
    sendResponse(client, response, sizeof(response));
}

void GVRETTCPServer::handleCommValidation(GVRETClient* client) {
    // Echo back the received byte
    uint8_t response[] = {0xF1, 0xE7, GVRETCommands::COMM_VALIDATION, client->rxBuffer[1]};
    sendResponse(client, response, sizeof(response));
}

void GVRETTCPServer::sendResponse(GVRETClient* client, const uint8_t* data, size_t length) {
    if (!client || !client->client.connected()) return;

    client->client.write(data, length);
    client->client.flush();
}

uint8_t GVRETTCPServer::calculateChecksum(const uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

uint32_t GVRETTCPServer::getMicroseconds() {
    uint32_t current = micros();
    // Handle rollover (approximately every 71 minutes)
    if (current < (_baseMicros & 0xFFFFFFFF)) {
        // Rollover detected, adjust base
        _baseMicros += 0x100000000;
    }
    return current - (_baseMicros & 0xFFFFFFFF);
}
