// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANProtocolEngine.cpp - Data-driven CAN protocol engine implementation
#include "CANProtocolEngine.h"
#include "CANConfigParser.h"
#include "EventLogger.h"
#include <ArduinoJson.h>

// Free functions defined in CANConfigParser.cpp (ArduinoJson types kept out of header)
extern uint8_t canConfigParseEngageRules(const JsonArray& steerArray,
                                          CANEngageRule* rules, uint8_t maxRules);
extern bool canConfigParseReceiveConfig(const JsonObject& canConfig, CANReceiveConfig& config);
extern bool canConfigParseSendConfig(const JsonObject& canConfig, CANSendConfig& config);

bool CANProtocolEngine::loadConfig(const char* json, size_t len, uint8_t brandId, uint8_t modelIndex) {
    configured = false;

    if (!json || len == 0) {
        LOG_ERROR(EventSource::AUTOSTEER, "CANProtocolEngine: No JSON data");
        return false;
    }

    // Parse JSON document (24KB matches SimpleWebManager pattern)
    DynamicJsonDocument doc(24576);
    DeserializationError error = deserializeJson(doc, json, len);

    if (error) {
        LOG_ERROR(EventSource::AUTOSTEER, "CANProtocolEngine: JSON parse error: %s", error.c_str());
        return false;
    }

    // Find the brand entry matching brandId
    JsonArray brands = doc["brands"];
    if (brands.isNull()) {
        LOG_ERROR(EventSource::AUTOSTEER, "CANProtocolEngine: No brands array in config");
        return false;
    }

    JsonObject brandObj;
    bool brandFound = false;
    for (JsonObject b : brands) {
        if ((uint8_t)(b["id"] | 0) == brandId) {
            brandObj = b;
            brandFound = true;
            break;
        }
    }

    if (!brandFound) {
        LOG_WARNING(EventSource::AUTOSTEER, "CANProtocolEngine: Brand %d not found in config", brandId);
        return false;
    }

    const char* brandName = brandObj["name"] | "Unknown";
    LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Loading config for %s (id=%d)", brandName, brandId);

    // Get canConfig - can be at brand level or model level
    // First check model-level canConfig (CAT MT uses this)
    JsonArray models = brandObj["models"];
    JsonObject canConfigObj;
    JsonObject selectedModel;
    bool hasModelConfig = false;

    if (!models.isNull() && models.size() > 0) {
        // Select model by index
        uint8_t idx = (modelIndex < models.size()) ? modelIndex : 0;
        selectedModel = models[idx];

        // Check if model has its own canConfig (overrides brand-level)
        if (selectedModel.containsKey("canConfig")) {
            canConfigObj = selectedModel["canConfig"];
            hasModelConfig = true;
            LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Using model-level canConfig for '%s'",
                     (const char*)(selectedModel["model"] | "?"));
        }
    }

    // Fall back to brand-level canConfig
    if (!hasModelConfig) {
        if (brandObj.containsKey("canConfig")) {
            canConfigObj = brandObj["canConfig"];
        } else {
            LOG_WARNING(EventSource::AUTOSTEER, "CANProtocolEngine: No canConfig found for brand %d", brandId);
            return false;
        }
    }

    // Parse VFilter for hardware mailbox filtering
    const char* vfilterStr = canConfigObj["VFilter"];
    if (vfilterStr) {
        filterCount = CANConfigParser::parseFilterIds(vfilterStr, filterIds, MAX_FILTER_IDS);
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: %d filter IDs parsed", filterCount);
    }

    // Parse receive config (valve status / curve feedback)
    if (canConfigParseReceiveConfig(canConfigObj, receiveConfig)) {
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Receive config - CAN 0x%08X, curve bytes %d,%d, valve byte %d",
                 receiveConfig.canId, receiveConfig.curveLoBytePos,
                 receiveConfig.curveHiBytePos, receiveConfig.valveStateBytePos);
    }

    // Parse send config (steering commands)
    if (canConfigParseSendConfig(canConfigObj, sendConfig)) {
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Send config - CAN 0x%08X, len %d, curve bytes %d,%d, curveMod %d",
                 sendConfig.canId, sendConfig.dataLen,
                 sendConfig.curveLoBytePos, sendConfig.curveHiBytePos, sendConfig.curveMod);
    }

    // Parse engage rules from all models' steer arrays
    engageRuleCount = 0;

    // Collect engage rules from the selected model, or all models if none selected
    if (!models.isNull()) {
        for (JsonObject model : models) {
            JsonArray steerArray = model["steer"];
            if (steerArray.isNull()) continue;

            uint8_t parsed = canConfigParseEngageRules(
                steerArray,
                &engageRules[engageRuleCount],
                MAX_ENGAGE_RULES - engageRuleCount);
            engageRuleCount += parsed;

            // Also add engage rule CAN IDs to filter list
            for (uint8_t i = engageRuleCount - parsed; i < engageRuleCount; i++) {
                if (filterCount < MAX_FILTER_IDS) {
                    // Check if already in filter list
                    bool exists = false;
                    for (uint8_t f = 0; f < filterCount; f++) {
                        if (filterIds[f] == engageRules[i].canId) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        filterIds[filterCount++] = engageRules[i].canId;
                    }
                }
            }
        }
    }

    LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: %d engage rules loaded, %d total filter IDs",
             engageRuleCount, filterCount);

    // Log each engage rule for debugging
    for (uint8_t i = 0; i < engageRuleCount; i++) {
        const CANEngageRule& rule = engageRules[i];
        LOG_INFO(EventSource::AUTOSTEER, "  Rule[%d]: CAN 0x%08X %s %d conditions \"%s\"",
                 i, rule.canId,
                 rule.useFallingEdge ? "falling" : "rising",
                 rule.conditionCount, rule.label);
    }

    configured = true;
    return true;
}

void CANProtocolEngine::processIncomingMessage(const CAN_message_t& msg) {
    if (!configured) return;

    processValveMessage(msg);
    processEngageRules(msg);
}

void CANProtocolEngine::processValveMessage(const CAN_message_t& msg) {
    if (!receiveConfig.configured) return;

    // Check if this message matches the receive config CAN ID
    if (msg.id != receiveConfig.canId) return;
    if (receiveConfig.isExtendedId != (bool)msg.flags.extended) return;

    valveDataReceived = true;

    // Extract curve value (little-endian by default)
    actualCurve = (int16_t)(msg.buf[receiveConfig.curveLoBytePos] |
                            (msg.buf[receiveConfig.curveHiBytePos] << 8));

    // Check valve ready from valve state byte
    bool newValveReady = (msg.buf[receiveConfig.valveStateBytePos] != 0);

    if (newValveReady != valveReady) {
        if (newValveReady) {
            LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Valve ready");
        } else {
            LOG_WARNING(EventSource::AUTOSTEER, "CANProtocolEngine: Valve not ready");
        }
    }

    valveReady = newValveReady;
    if (valveReady) {
        lastValveReadyTime = millis();
    }
}

void CANProtocolEngine::processEngageRules(const CAN_message_t& msg) {
    for (uint8_t i = 0; i < engageRuleCount; i++) {
        CANEngageRule& rule = engageRules[i];

        // Check CAN ID match
        if (msg.id != rule.canId) continue;
        if (rule.isExtendedId != (bool)msg.flags.extended) continue;

        // Evaluate all conditions (AND logic)
        bool allMatch = true;
        for (uint8_t c = 0; c < rule.conditionCount; c++) {
            const CANByteCondition& cond = rule.conditions[c];
            if (cond.byteIndex >= msg.len) {
                allMatch = false;
                break;
            }
            if ((msg.buf[cond.byteIndex] & cond.mask) != cond.expectedValue) {
                allMatch = false;
                break;
            }
        }

        // Update state
        rule.previousState = rule.currentState;
        rule.currentState = allMatch;

        // Detect edge
        if (rule.useFallingEdge) {
            // Falling edge: was true, now false
            if (rule.previousState && !rule.currentState) {
                rule.eventTriggered = true;
                LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Engage event (falling) - %s", rule.label);
            }
        } else {
            // Rising edge: was false, now true
            if (!rule.previousState && rule.currentState) {
                rule.eventTriggered = true;
                LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Engage event (rising) - %s", rule.label);
            }
        }
    }
}

void CANProtocolEngine::sendSteerCommand(int16_t curve, bool steerActive, uint8_t busNum) {
    if (!configured || !sendConfig.configured) return;
    if (busNum == 0) return;

    CAN_message_t msg;
    msg.id = sendConfig.canId;
    msg.flags.extended = sendConfig.isExtendedId ? 1 : 0;
    msg.len = sendConfig.dataLen;

    // Start from the appropriate template
    if (steerActive) {
        memcpy(msg.buf, sendConfig.templateSteer, 8);
    } else {
        memcpy(msg.buf, sendConfig.templateNoSteer, 8);
    }

    // Apply curve value with curveMod offset
    int16_t adjustedCurve = curve + sendConfig.curveMod;

    // Insert curve bytes
    msg.buf[sendConfig.curveLoBytePos] = adjustedCurve & 0xFF;
    msg.buf[sendConfig.curveHiBytePos] = (adjustedCurve >> 8) & 0xFF;

    // For non-separate templates, intent is already baked in from parsing.
    // For separate templates, the template itself defines intent.

    writeCANMessage(busNum, msg);
}

bool CANProtocolEngine::checkEngageEvent() {
    for (uint8_t i = 0; i < engageRuleCount; i++) {
        if (engageRules[i].eventTriggered) {
            // Copy label before clearing
            strncpy(lastEngageLabel, engageRules[i].label, sizeof(lastEngageLabel) - 1);
            engageRules[i].eventTriggered = false;
            return true;
        }
    }
    return false;
}

void CANProtocolEngine::resetEngageFlags() {
    for (uint8_t i = 0; i < engageRuleCount; i++) {
        engageRules[i].eventTriggered = false;
    }
}

void CANProtocolEngine::writeCANMessage(uint8_t busNum, const CAN_message_t& msg) {
    switch (busNum) {
        case 1: globalCAN1.write(msg); break;
        case 2: globalCAN2.write(msg); break;
        case 3: globalCAN3.write(msg); break;
    }
}
