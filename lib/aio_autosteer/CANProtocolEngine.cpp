// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANProtocolEngine.cpp - Data-driven CAN protocol engine implementation
#include "CANProtocolEngine.h"
#include "CANConfigParser.h"
#include "EventLogger.h"
#include "MessageBuilder.h"
#include <ArduinoJson.h>
#include <cstring>

// Free functions defined in CANConfigParser.cpp (ArduinoJson types kept out of header)
extern uint8_t canConfigParseEngageRules(const JsonArray& steerArray,
                                          CANEngageRule* rules, uint8_t maxRules);
extern bool canConfigParseReceiveConfig(const JsonObject& canConfig, CANReceiveConfig& config);
extern bool canConfigParseSendConfig(const JsonObject& canConfig, CANSendConfig& config);
extern bool canConfigParseKickoutConfig(const JsonObject& kickoutObj, CANKickoutConfig& config);
extern bool canConfigParseErrorMessages(const JsonObject& errorObj, CANErrorMessageConfig* configs, uint8_t maxCount);

bool CANProtocolEngine::loadConfig(const char* json, size_t len, uint8_t brandId, uint8_t modelIndex) {
    configured = false;

    if (!json || len == 0) {
        LOG_ERROR(EventSource::AUTOSTEER, "CANProtocolEngine: No JSON data");
        return false;
    }

    // Parse JSON document (24KB matches SimpleWebManager pattern)
    DynamicJsonDocument doc(24576);
    DeserializationError error = deserializeJson(doc, json, len,
                                                  DeserializationOption::NestingLimit(20));

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
    setBrandName(brandName);
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
        if (!selectedModel["canConfig"].isNull()) {
            canConfigObj = selectedModel["canConfig"];
            hasModelConfig = true;
            LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Using model-level canConfig for '%s'",
                     (const char*)(selectedModel["model"] | "?"));
        }
    }

    // Fall back to brand-level canConfig
    if (!hasModelConfig) {
        if (!brandObj["canConfig"].isNull()) {
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

    // Parse kickout detection configuration (from brand level, can also be at model level)
    JsonObject kickoutObj = canConfigObj["kickoutDetection"];
    if (kickoutObj.isNull() && hasModelConfig) {
        // Try model-level kickout config
        kickoutObj = selectedModel["kickoutDetection"];
    }
    if (!kickoutObj.isNull() && canConfigParseKickoutConfig(kickoutObj, kickoutConfig)) {
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Kickout detection enabled - CAN 0x%08X, byte %d",
                 kickoutConfig.canId, kickoutConfig.statusByte);
    }

    // Parse error messages configuration (from brand level)
    JsonObject errorObj = brandObj["errorMessages"];
    if (errorObj.isNull()) {
        // Try canConfig level
        errorObj = canConfigObj["errorMessages"];
    }
    if (!errorObj.isNull()) {
        errorMessageCount = canConfigParseErrorMessages(errorObj, errorMessages, MAX_ERROR_MESSAGES);
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: %d error messages loaded", errorMessageCount);
    }

    // Parse V-Bus engage rules from selected model (for MF 7600 etc)
    if (hasModelConfig && !selectedModel["vBusEngage"].isNull()) {
        JsonObject vBusEngageObj = selectedModel["vBusEngage"];
        if (vBusEngageObj["enabled"] | false) {
            JsonArray rules = vBusEngageObj["rules"];
            if (!rules.isNull()) {
                for (JsonObject ruleJson : rules) {
                    if (engageRuleCount >= MAX_ENGAGE_RULES) break;

                    CANEngageRule& rule = engageRules[engageRuleCount];
                    rule = {};  // Zero-initialize (safer than memset for structs with bool members)

                    // Parse CAN ID
                    const char* canIdStr = ruleJson["canId"];
                    if (canIdStr) {
                        rule.canId = CANConfigParser::parseHexString(canIdStr);
                        rule.isExtendedId = (rule.canId > 0x7FF);

                        // Parse label
                        const char* label = ruleJson["name"];
                        if (label) {
                            strncpy(rule.label, label, sizeof(rule.label) - 1);
                        }

                        // Parse multi-byte conditions
                        JsonArray bytes = ruleJson["matchCondition"]["bytes"];
                        if (!bytes.isNull()) {
                            rule.conditionCount = 0;
                            for (JsonObject byteJson : bytes) {
                                if (rule.conditionCount >= MAX_CONDITIONS_PER_RULE) break;
                                CANByteCondition& bc = rule.conditions[rule.conditionCount];
                                bc.byteIndex = byteJson["index"] | 0;
                                bc.mask = 0xFF;  // Multi-byte uses exact match
                                bc.expectedValue = byteJson["value"] | 0;
                                rule.conditionCount++;
                            }
                        }

                        // Rising edge for engage
                        rule.useFallingEdge = false;

                        // Add to filter list
                        if (filterCount < MAX_FILTER_IDS) {
                            bool exists = false;
                            for (uint8_t f = 0; f < filterCount; f++) {
                                if (filterIds[f] == rule.canId) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) {
                                filterIds[filterCount++] = rule.canId;
                            }
                        }

                        LOG_INFO(EventSource::AUTOSTEER,
                                 "  V-Bus Engage Rule: CAN 0x%08X, %d bytes, \"%s\"",
                                 rule.canId, rule.conditionCount, rule.label);

                        engageRuleCount++;
                    }
                }
            }
        }
    }

    // Parse engage rules from all models' steer arrays (append to any V-Bus engage rules already parsed)
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

    // Process kickout detection
    if (kickoutConfig.configured) {
        processKickoutDetection(msg, autosteerActive);
    }
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
                vBusEngaged = true;  // Set V-Bus engaged state
                vBusEngageTimeout = millis() + 5000;  // 5 second timeout
                LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: Engage event (rising) - %s - V-Bus ENGAGED", rule.label);
            }
        }
    }

    // Check for V-Bus engage timeout (5 seconds without re-trigger)
    if (vBusEngaged && millis() > vBusEngageTimeout) {
        vBusEngaged = false;
        LOG_INFO(EventSource::AUTOSTEER, "CANProtocolEngine: V-Bus engage timeout - disengaged");
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

// === Kickout Detection ===
void CANProtocolEngine::processKickoutDetection(const CAN_message_t& msg, bool autosteerActive) {
    if (!kickoutConfig.configured) return;

    // Check if this is the kickout detection message
    if (msg.id != kickoutConfig.canId) return;

    // Only check when autosteer is active (if configured)
    if (kickoutConfig.onlyWhenEngaged && !autosteerActive) return;

    // Check hysteresis
    if (millis() - lastKickoutTime < kickoutConfig.hysteresisMs) return;

    if (kickoutConfig.statusByte >= msg.len) return;

    uint8_t status = msg.buf[kickoutConfig.statusByte];
    bool isReady = kickoutConfig.isReady(status);

    // Check for edge transition from ready to not ready (kickout)
    if (lastReadyState && !isReady) {
        kickoutDetected = true;
        lastKickoutTime = millis();

        LOG_WARNING(EventSource::AUTOSTEER,
                  "CANProtocolEngine: Kickout detected! CAN 0x%08X, byte %d = %d (was ready)",
                  msg.id, kickoutConfig.statusByte, status);

        // Send error message
        sendError("kickout", String(status).c_str());
    }

    lastReadyState = isReady;
}

// === Error Messages ===
void CANProtocolEngine::sendError(const char* errorKey, const char* extra) {
    const CANErrorMessageConfig* cfg = getErrorConfig(errorKey);
    if (cfg && cfg->configured) {
        // First pass: replace {brand}
        char temp[64];
        const char* tmpl = cfg->messageTemplate;
        const char* brandPtr = strstr(tmpl, "{brand}");
        if (brandPtr) {
            int prefixLen = brandPtr - tmpl;
            snprintf(temp, sizeof(temp), "%.*s%s%s", prefixLen, tmpl, brandName, brandPtr + 7);
        } else {
            snprintf(temp, sizeof(temp), "%s", tmpl);
        }

        // Second pass: replace {extra}
        char message[64];
        const char* extraPtr = strstr(temp, "{extra}");
        if (extraPtr && extra && extra[0] != '\0') {
            int prefixLen = extraPtr - temp;
            snprintf(message, sizeof(message), "%.*s%s%s", prefixLen, temp, extra, extraPtr + 7);
        } else if (extraPtr) {
            // Remove {extra} placeholder
            int prefixLen = extraPtr - temp;
            snprintf(message, sizeof(message), "%.*s%s", prefixLen, temp, extraPtr + 7);
        } else {
            snprintf(message, sizeof(message), "%s", temp);
        }

        MessageBuilder::sendHardwarePopup(message, cfg->duration, cfg->color);
    } else {
        // Fallback if error key not found
        if (extra && extra[0] != '\0') {
            MessageBuilder::sendHardwarePopup(extra, 3, 1);
        }
    }
}

const CANErrorMessageConfig* CANProtocolEngine::getErrorConfig(const char* key) const {
    for (uint8_t i = 0; i < errorMessageCount; i++) {
        if (strcmp(errorMessages[i].key, key) == 0) {
            return &errorMessages[i];
        }
    }
    return nullptr;
}
