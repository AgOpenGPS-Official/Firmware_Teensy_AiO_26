// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// GVRETTCPServer.h
// GVRET (GVRET/Automotive) protocol TCP server implementation
// Provides CAN bus monitoring and transmission via TCP for SavvyCAN and other tools

#ifndef GVRET_TCP_SERVER_H
#define GVRET_TCP_SERVER_H

#include <Arduino.h>
#include <QNEthernet.h>
#include <vector>
#include <memory>

#include <FlexCAN_T4.h>

using namespace qindesign::network;

// GVRET Protocol Command Codes
namespace GVRETCommands {
    constexpr uint8_t BINARY_MODE = 0xE7;          // Switch to binary mode
    constexpr uint8_t SEND_CAN_FRAME = 0x00;       // Send CAN frame (from client)
    constexpr uint8_t TIME_SYNC = 0x01;            // Time synchronization
    constexpr uint8_t GET_DIGITAL_INPUTS = 0x03;   // Read digital inputs
    constexpr uint8_t SET_DIGITAL_OUTPUTS = 0x04;  // Set digital outputs
    constexpr uint8_t GET_ANALOG_INPUTS = 0x05;    // Read analog inputs
    constexpr uint8_t GET_CANBUS_STATS = 0x06;     // Get CAN bus statistics
    constexpr uint8_t GET_DEVICE_INFO = 0x07;      // Get device info string
    constexpr uint8_t SET_SINGLE_WIRE_MODE = 0x08; // Single wire CAN mode
    constexpr uint8_t COMM_VALIDATION = 0x09;      // Echo test for validation
    constexpr uint8_t SET_NV_BLOCK = 0x0A;         // Set non-volatile memory
    constexpr uint8_t GET_NV_BLOCK = 0x0B;         // Get non-volatile memory
    constexpr uint8_t SETUP_CAN_BUS = 0x0C;        // Setup CAN bus speed
    constexpr uint8_t KEEP_ALIVE = 0x0D;           // Keep alive command
    constexpr uint8_t SET_LOGGING = 0x0E;          // Enable/disable logging
    constexpr uint8_t MAC_ADDRESS_QUERY = 0xF1;    // Get MAC address
}

// GVRET frame prefix for binary mode
constexpr uint8_t GVRET_FRAME_PREFIX[] = {0xF1, 0xE7};

// GVRET CAN Frame Structure (to client):
// Byte 0-3:   Timestamp (microseconds, little-endian)
// Byte 4-7:   CAN ID (little-endian)
// Byte 8:     Flags (extended flag in bit 7, data length in bits 0-3)
// Byte 9-16:  Data bytes (0-8)
// Last Byte:  Checksum

// Per-client state for protocol parsing
struct GVRETClient {
    EthernetClient client;
    uint32_t lastActivity;
    uint8_t busNum;  // Which CAN bus this client is connected to (0, 1, 2)

    // RX state machine
    enum class RXState {
        WAIT_PREFIX,     // Waiting for 0xF1 0xE7 prefix
        WAIT_COMMAND,    // Waiting for command byte
        WAIT_DATA        // Waiting for command-specific data
    };
    RXState rxState;

    uint8_t rxBuffer[256];
    uint16_t rxIndex;
    uint8_t expectedBytes;
    bool binaryMode;

    GVRETClient() : lastActivity(0), busNum(0), rxState(RXState::WAIT_PREFIX),
                    rxIndex(0), expectedBytes(0), binaryMode(false) {}
};

// GVRET TCP Server for CAN monitoring
class GVRETTCPServer {
public:
    GVRETTCPServer();
    ~GVRETTCPServer();

    // Server control
    bool begin();  // Start servers on ports 2201, 2202, 2203
    void stop();   // Stop all servers
    void handleClients();  // Call from scheduler (HZ_100)

    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Get connection counts per bus
    size_t getClientCount(uint8_t busNum) const;

    // Statistics
    struct Stats {
        uint32_t framesReceived[3];   // CAN frames received per bus
        uint32_t framesSent[3];       // CAN frames sent per bus
        uint32_t bytesReceived[3];    // TCP bytes received per bus
        uint32_t bytesSent[3];        // TCP bytes sent per bus
        uint32_t connections[3];      // Total connections per bus
        uint32_t errors;              // Protocol errors
    };
    const Stats& getStats() const { return _stats; }
    void resetStats();

private:
    EthernetServer _servers[3];  // Ports 2201, 2202, 2203 for CAN1, CAN2, CAN3
    std::vector<std::unique_ptr<GVRETClient>> _clients[3];
    bool _enabled;
    Stats _stats;
    uint32_t _baseMicros;  // Base timestamp for microsecond rollover handling

    // Client management
    void acceptNewClients(uint8_t busNum);
    void removeDisconnectedClients(uint8_t busNum);
    void processClientData(GVRETClient* client, uint8_t busNum);

    // CAN frame polling and transmission
    void pollCANBuses();
    void sendFrameToClients(uint8_t busNum, const CAN_message_t& msg);

    // GVRET Command handlers
    void handleBinaryMode(GVRETClient* client);
    void handleSendCANFrame(GVRETClient* client, uint8_t busNum);
    void handleTimeSync(GVRETClient* client);
    void handleGetCANBusStats(GVRETClient* client, uint8_t busNum);
    void handleGetDeviceInfo(GVRETClient* client);
    void handleSetupCanBus(GVRETClient* client);
    void handleCommValidation(GVRETClient* client);

    // Helper methods
    void sendResponse(GVRETClient* client, const uint8_t* data, size_t length);
    uint8_t calculateChecksum(const uint8_t* data, size_t length);
    uint32_t getMicroseconds();  // Get 32-bit microsecond timestamp
    uint8_t getExpectedDataLength(uint8_t command);  // Get expected data length for command
    void processCommand(GVRETClient* client, uint8_t busNum);  // Process complete GVRET command
};

#endif // GVRET_TCP_SERVER_H
