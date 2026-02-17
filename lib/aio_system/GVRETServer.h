// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// GVRETServer.h - GVRET binary protocol over TCP for SavvyCAN compatibility
// Single server on port 23 serves all 3 CAN buses.
// Bus number is encoded in each frame. Connect with SavvyCAN → Network Connection (GVRET).

#ifndef GVRET_SERVER_H
#define GVRET_SERVER_H

#include <Arduino.h>
#include <QNEthernet.h>

using namespace qindesign::network;

class GVRETServer {
public:
    GVRETServer();

    void begin();
    void loop();

    // Called from CAN RX/TX path — must be fast
    // busNum: 0=CAN1, 1=CAN2, 2=CAN3
    void sendFrame(uint8_t busNum, uint32_t id, const uint8_t* data, uint8_t len,
                   bool extended, bool isTx);

    bool hasClient() { return client.connected(); }

private:
    EthernetServer server;
    EthernetClient client;
    bool binaryMode = false;

    // Transmit buffer — aggregate frames, flush periodically
    static constexpr size_t TX_BUF_SIZE = 2048;
    uint8_t txBuf[TX_BUF_SIZE];
    size_t txBufLen = 0;
    uint32_t lastFlushMicros = 0;

    // Protocol state machine
    enum State : uint8_t {
        IDLE,
        GET_COMMAND,
        BUILD_CAN_FRAME,
        SET_SINGLEWIRE,
        SET_CANBUS_PARAMS,
        SETUP_EXT_BUSES
    };
    State state = IDLE;
    uint8_t cmdBuf[20];
    uint8_t step = 0;

    void processIncomingByte(uint8_t b);
    void flushBuffer();
    void respondDeviceInfo();
    void respondBusParams();
    void respondKeepalive();
    void respondTimeSync();
    void respondNumBuses();
};

#endif // GVRET_SERVER_H
