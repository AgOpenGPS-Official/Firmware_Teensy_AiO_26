// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// GVRETServer.cpp - GVRET binary protocol over TCP
// Implements enough of the ESP32RET/GVRET protocol for SavvyCAN to connect
// and receive CAN frames in real time.

#include "GVRETServer.h"
#include "EventLogger.h"

// GVRET protocol constants
static constexpr uint8_t PROTO_BUILD_CAN_FRAME  = 0x00;
static constexpr uint8_t PROTO_TIME_SYNC        = 0x01;
static constexpr uint8_t PROTO_GET_DIG_INPUTS   = 0x02;
static constexpr uint8_t PROTO_GET_ANALOG_INPUTS = 0x03;
static constexpr uint8_t PROTO_SET_DIG_OUTPUTS   = 0x04;
static constexpr uint8_t PROTO_SETUP_CANBUS      = 0x06;
static constexpr uint8_t PROTO_GET_CANBUS_PARAMS = 0x06;
static constexpr uint8_t PROTO_GET_DEV_INFO      = 0x07;
static constexpr uint8_t PROTO_SET_SINGLEWIRE    = 0x08;
static constexpr uint8_t PROTO_KEEPALIVE         = 0x09;
static constexpr uint8_t PROTO_SET_SYSYPE        = 0x0A;
static constexpr uint8_t PROTO_ECHO_CAN_FRAME    = 0x0B;
static constexpr uint8_t PROTO_GET_NUMBUSES      = 0x0C;
static constexpr uint8_t PROTO_GET_EXT_BUSES     = 0x0D;
static constexpr uint8_t PROTO_SET_EXT_BUSES     = 0x0E;

static constexpr uint8_t MARKER = 0xF1;
static constexpr uint8_t BINARY_MODE_MARKER = 0xE7;

// Build number reported to SavvyCAN (matches ESP32RET convention)
static constexpr uint16_t BUILD_NUMBER = 6020;
// Device type: MACCHINA_ESP32 is what SavvyCAN expects for network GVRET
static constexpr uint8_t DEVICE_TYPE = 7;

GVRETServer::GVRETServer()
    : server(23) {}

void GVRETServer::begin() {
    server.begin();
    LOG_INFO(EventSource::SYSTEM, "GVRET server started on port 23 (all CAN buses)");
}

void GVRETServer::loop() {
    // Accept new client if none connected
    if (!client.connected()) {
        EthernetClient newClient = server.available();
        if (newClient) {
            client = newClient;
            binaryMode = false;
            txBufLen = 0;
            state = IDLE;
            LOG_INFO(EventSource::SYSTEM, "SavvyCAN client connected");
        }
        return;  // Nothing else to do without a client
    }

    // Process incoming bytes from SavvyCAN
    while (client.available()) {
        processIncomingByte(client.read());
    }

    // Periodic flush: every 20ms or buffer near-full
    if (txBufLen > 0) {
        uint32_t now = micros();
        if ((now - lastFlushMicros > 20000) || (txBufLen > TX_BUF_SIZE - 40)) {
            flushBuffer();
        }
    }
}

void GVRETServer::sendFrame(uint8_t busNum, uint32_t id, const uint8_t* data,
                             uint8_t len, bool extended, bool isTx) {
    if (!client.connected() || !binaryMode) return;

    size_t needed = 12 + len;  // marker(1) + cmd(1) + ts(4) + id(4) + len_bus(1) + data(len) + cksum(1)
    if (txBufLen + needed > TX_BUF_SIZE) {
        flushBuffer();
    }

    uint32_t now = micros();
    uint32_t wireId = extended ? (id | (1UL << 31)) : id;

    txBuf[txBufLen++] = MARKER;
    txBuf[txBufLen++] = PROTO_BUILD_CAN_FRAME;
    // Timestamp LE
    txBuf[txBufLen++] = now & 0xFF;
    txBuf[txBufLen++] = (now >> 8) & 0xFF;
    txBuf[txBufLen++] = (now >> 16) & 0xFF;
    txBuf[txBufLen++] = (now >> 24) & 0xFF;
    // ID LE
    txBuf[txBufLen++] = wireId & 0xFF;
    txBuf[txBufLen++] = (wireId >> 8) & 0xFF;
    txBuf[txBufLen++] = (wireId >> 16) & 0xFF;
    txBuf[txBufLen++] = (wireId >> 24) & 0xFF;
    // Length (low nibble) | bus number (high nibble)
    txBuf[txBufLen++] = (len & 0x0F) | ((busNum & 0x0F) << 4);
    // Data bytes
    memcpy(&txBuf[txBufLen], data, len);
    txBufLen += len;
    // Checksum placeholder (ESP32RET sends 0)
    txBuf[txBufLen++] = 0;
}

void GVRETServer::flushBuffer() {
    if (txBufLen == 0 || !client.connected()) return;
    client.write(txBuf, txBufLen);
    txBufLen = 0;
    lastFlushMicros = micros();
}

void GVRETServer::processIncomingByte(uint8_t b) {
    switch (state) {
        case IDLE:
            if (b == BINARY_MODE_MARKER) {
                binaryMode = true;
            } else if (b == MARKER) {
                state = GET_COMMAND;
            }
            break;

        case GET_COMMAND:
            switch (b) {
                case PROTO_TIME_SYNC:
                    respondTimeSync();
                    state = IDLE;
                    break;

                case PROTO_GET_DIG_INPUTS:
                    // Respond with empty digital inputs: F1 02 00 00
                    {
                        uint8_t resp[] = {MARKER, PROTO_GET_DIG_INPUTS, 0, 0};
                        client.write(resp, sizeof(resp));
                    }
                    state = IDLE;
                    break;

                case PROTO_GET_ANALOG_INPUTS:
                    // Respond with zeros: F1 03 + 8 zero bytes
                    {
                        uint8_t resp[10] = {MARKER, PROTO_GET_ANALOG_INPUTS};
                        client.write(resp, sizeof(resp));
                    }
                    state = IDLE;
                    break;

                case PROTO_SET_DIG_OUTPUTS:
                    // Consume 1 byte (output state), ignore
                    state = IDLE;  // Next byte is data, but we just skip it
                    break;

                case PROTO_GET_CANBUS_PARAMS:
                    respondBusParams();
                    state = IDLE;
                    break;

                case PROTO_GET_DEV_INFO:
                    respondDeviceInfo();
                    state = IDLE;
                    break;

                case PROTO_SET_SINGLEWIRE:
                    // Need to consume 1 byte
                    step = 0;
                    state = SET_SINGLEWIRE;
                    break;

                case PROTO_KEEPALIVE:
                    respondKeepalive();
                    state = IDLE;
                    break;

                case PROTO_SET_SYSYPE:
                    // Consume 1 byte (system type), ignore
                    state = IDLE;
                    break;

                case PROTO_ECHO_CAN_FRAME:
                    // Echo mode toggle — ignore
                    state = IDLE;
                    break;

                case PROTO_GET_NUMBUSES:
                    respondNumBuses();
                    state = IDLE;
                    break;

                case PROTO_GET_EXT_BUSES:
                    // Respond with no extended buses
                    {
                        uint8_t resp[] = {MARKER, PROTO_GET_EXT_BUSES, 0, 0};
                        client.write(resp, sizeof(resp));
                    }
                    state = IDLE;
                    break;

                case PROTO_SET_EXT_BUSES:
                    // Need to consume 2 bytes
                    step = 0;
                    state = SETUP_EXT_BUSES;
                    break;

                case PROTO_BUILD_CAN_FRAME:
                    // SavvyCAN sending us a CAN frame to transmit
                    // We need to consume: 4 bytes ID + 1 byte len_bus + up to 8 data + 1 checksum
                    step = 0;
                    state = BUILD_CAN_FRAME;
                    break;

                default:
                    // Unknown command, return to idle
                    state = IDLE;
                    break;
            }
            break;

        case BUILD_CAN_FRAME:
            // Consume the incoming CAN frame (read-only sniffer, don't transmit)
            // Format: id[4] + len_bus[1] + data[0-8] + checksum[1]
            cmdBuf[step++] = b;
            if (step == 5) {
                // We now know the data length from byte 4 (low nibble)
                // Total remaining = data_len + 1 (checksum)
            }
            if (step >= 5) {
                uint8_t dataLen = cmdBuf[4] & 0x0F;
                // 5 bytes header consumed, need dataLen + 1 more
                if (step >= (uint8_t)(5 + dataLen + 1)) {
                    state = IDLE;
                }
            }
            if (step >= sizeof(cmdBuf)) {
                state = IDLE;  // Safety: don't overflow
            }
            break;

        case SET_SINGLEWIRE:
            // Consume 1 byte, ignore
            state = IDLE;
            break;

        case SET_CANBUS_PARAMS:
            // Consume 10 bytes (speed + flags for 2 buses), ignore
            cmdBuf[step++] = b;
            if (step >= 10) {
                state = IDLE;
            }
            break;

        case SETUP_EXT_BUSES:
            // Consume 2 bytes, ignore
            cmdBuf[step++] = b;
            if (step >= 2) {
                state = IDLE;
            }
            break;
    }
}

void GVRETServer::respondTimeSync() {
    uint32_t now = micros();
    uint8_t resp[6];
    resp[0] = MARKER;
    resp[1] = PROTO_TIME_SYNC;
    resp[2] = now & 0xFF;
    resp[3] = (now >> 8) & 0xFF;
    resp[4] = (now >> 16) & 0xFF;
    resp[5] = (now >> 24) & 0xFF;
    client.write(resp, sizeof(resp));
}

void GVRETServer::respondDeviceInfo() {
    // Format: F1 07 build_lo build_hi 0x20 single_wire device_type
    uint8_t resp[7];
    resp[0] = MARKER;
    resp[1] = PROTO_GET_DEV_INFO;
    resp[2] = BUILD_NUMBER & 0xFF;
    resp[3] = (BUILD_NUMBER >> 8) & 0xFF;
    resp[4] = 0x20;  // Version character
    resp[5] = 0;     // Single-wire mode: no
    resp[6] = DEVICE_TYPE;
    client.write(resp, sizeof(resp));
}

void GVRETServer::respondBusParams() {
    // Report 3 buses with CAN speed 500000 and enabled
    // Format: F1 06 + for each bus: enabled(1) + speed(4) = 5 bytes per bus
    uint8_t resp[17];
    resp[0] = MARKER;
    resp[1] = PROTO_GET_CANBUS_PARAMS;
    uint32_t speed = 500000;
    // Bus 0 (CAN1)
    resp[2] = 1;
    resp[3] = speed & 0xFF;
    resp[4] = (speed >> 8) & 0xFF;
    resp[5] = (speed >> 16) & 0xFF;
    resp[6] = (speed >> 24) & 0xFF;
    // Bus 1 (CAN2)
    resp[7] = 1;
    resp[8] = speed & 0xFF;
    resp[9] = (speed >> 8) & 0xFF;
    resp[10] = (speed >> 16) & 0xFF;
    resp[11] = (speed >> 24) & 0xFF;
    // Bus 2 (CAN3)
    resp[12] = 1;
    resp[13] = speed & 0xFF;
    resp[14] = (speed >> 8) & 0xFF;
    resp[15] = (speed >> 16) & 0xFF;
    resp[16] = (speed >> 24) & 0xFF;
    client.write(resp, sizeof(resp));
}

void GVRETServer::respondKeepalive() {
    uint8_t resp[] = {MARKER, PROTO_KEEPALIVE, 0xDE, 0xAD};
    client.write(resp, sizeof(resp));
}

void GVRETServer::respondNumBuses() {
    uint8_t resp[] = {MARKER, PROTO_GET_NUMBUSES, 3};  // 3 CAN buses
    client.write(resp, sizeof(resp));
}
