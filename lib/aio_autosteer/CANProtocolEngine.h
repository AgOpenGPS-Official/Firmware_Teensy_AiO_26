// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANProtocolEngine.h - Data-driven CAN protocol handler
// Reads JSON configs at runtime to replace per-brand C++ code
#ifndef CAN_PROTOCOL_ENGINE_H
#define CAN_PROTOCOL_ENGINE_H

#include <Arduino.h>
#include "CANGlobals.h"

// Maximum limits for config arrays
static const uint8_t MAX_ENGAGE_RULES = 16;
static const uint8_t MAX_CONDITIONS_PER_RULE = 4;
static const uint8_t MAX_FILTER_IDS = 16;

// A single byte-level condition within an engage rule
struct CANByteCondition {
    uint8_t byteIndex;
    uint8_t mask;
    uint8_t expectedValue;
};

// An engage/disengage rule parsed from JSON steer[] entries
struct CANEngageRule {
    uint32_t canId;
    bool isExtendedId;
    CANByteCondition conditions[MAX_CONDITIONS_PER_RULE];
    uint8_t conditionCount;
    bool useFallingEdge;
    char label[48];

    // Runtime state
    bool currentState;
    bool previousState;
    bool eventTriggered;
};

// Configuration for receiving valve/curve status messages
struct CANReceiveConfig {
    uint32_t canId;
    bool isExtendedId;
    uint8_t curveLoBytePos;
    uint8_t curveHiBytePos;
    uint8_t valveStateBytePos;
    bool configured;
};

// Configuration for sending steering commands
struct CANSendConfig {
    uint32_t canId;
    bool isExtendedId;
    uint8_t dataLen;
    uint8_t templateSteer[8];
    uint8_t templateNoSteer[8];
    uint8_t curveLoBytePos;
    uint8_t curveHiBytePos;
    uint8_t intentBytePos;
    uint8_t intentEnabled;
    uint8_t intentDisabled;
    int16_t curveMod;
    bool hasSeperateTemplates;  // Fendt uses distinct steer/no-steer templates
    bool configured;
};

class CANProtocolEngine {
public:
    CANProtocolEngine() {}

    // Load and parse a JSON config for the given brand+model IDs
    // Returns true if config was successfully parsed
    bool loadConfig(const char* json, size_t len, uint8_t brandId, uint8_t modelIndex = 0);

    // Process a single incoming CAN message (call for each message from any bus)
    void processIncomingMessage(const CAN_message_t& msg);

    // Build and send a steering command
    // busNum: which CAN bus to send on (1, 2, or 3)
    void sendSteerCommand(int16_t curve, bool steerActive, uint8_t busNum);

    // Check if any engage rule fired since last call
    // Returns true once per event, then resets the triggered flag
    bool checkEngageEvent();

    // Get the label of the last triggered engage rule
    const char* getLastEngageLabel() const { return lastEngageLabel; }

    // Reset all engage event flags
    void resetEngageFlags();

    // Valve/motor readiness
    bool isValveReady() const { return valveReady; }
    bool isValveDataReceived() const { return valveDataReceived; }
    int16_t getActualCurve() const { return actualCurve; }

    // Filter IDs for hardware mailbox setup
    uint8_t getFilterCount() const { return filterCount; }
    const uint32_t* getFilterIds() const { return filterIds; }

    // Engage rule count (for logging)
    uint8_t getEngageRuleCount() const { return engageRuleCount; }

    // Is the engine configured and ready?
    bool isConfigured() const { return configured; }

private:
    // Parsed configuration
    CANReceiveConfig receiveConfig = {};
    CANSendConfig sendConfig = {};
    CANEngageRule engageRules[MAX_ENGAGE_RULES] = {};
    uint8_t engageRuleCount = 0;
    uint32_t filterIds[MAX_FILTER_IDS] = {};
    uint8_t filterCount = 0;

    // Runtime state
    bool configured = false;
    bool valveReady = false;
    bool valveDataReceived = false;
    int16_t actualCurve = 0;
    uint32_t lastValveReadyTime = 0;
    char lastEngageLabel[48] = {};

    // Internal helpers
    void processValveMessage(const CAN_message_t& msg);
    void processEngageRules(const CAN_message_t& msg);
    void writeCANMessage(uint8_t busNum, const CAN_message_t& msg);
};

#endif // CAN_PROTOCOL_ENGINE_H
