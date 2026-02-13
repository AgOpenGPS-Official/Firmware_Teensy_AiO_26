// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANConfigParser.cpp - JSON parsing implementation for CAN configuration
#include "CANConfigParser.h"
#include "EventLogger.h"
#include <ArduinoJson.h>

uint32_t CANConfigParser::parseHexString(const char* str) {
    if (!str) return 0;
    // Skip "0x" or "0X" prefix if present
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }
    return strtoul(str, nullptr, 16);
}

void CANConfigParser::parseCommaSeparatedBytes(const char* str, uint8_t* out, uint8_t& count) {
    count = 0;
    if (!str || !out) return;

    const char* p = str;
    while (*p && count < 8) {
        // Skip whitespace
        while (*p == ' ') p++;
        if (*p == '\0') break;

        // Check for hex prefix
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            out[count++] = (uint8_t)strtoul(p, nullptr, 16);
        } else {
            out[count++] = (uint8_t)atoi(p);
        }

        // Skip to next comma or end
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
}

void CANConfigParser::parseBytePositions(const char* str, uint8_t& lo, uint8_t& hi) {
    lo = 0;
    hi = 1;
    if (!str) return;

    lo = (uint8_t)atoi(str);
    const char* comma = strchr(str, ',');
    if (comma) {
        hi = (uint8_t)atoi(comma + 1);
    }
}

void CANConfigParser::parseIntentString(const char* str, uint8_t& bytePos, uint8_t& value) {
    bytePos = 0;
    value = 0;
    if (!str) return;

    bytePos = (uint8_t)atoi(str);
    const char* comma = strchr(str, ',');
    if (comma) {
        value = (uint8_t)atoi(comma + 1);
    }
}

uint8_t CANConfigParser::parseFilterIds(const char* vfilterStr, uint32_t* ids, uint8_t maxIds) {
    uint8_t count = 0;
    if (!vfilterStr || !ids) return 0;

    const char* p = vfilterStr;
    while (*p && count < maxIds) {
        // Skip whitespace
        while (*p == ' ') p++;
        if (*p == '\0') break;

        ids[count++] = parseHexString(p);

        // Skip to next comma or end
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }

    return count;
}

// --- JSON-dependent functions (use ArduinoJson types) ---
// These are free functions, not class members, to keep ArduinoJson out of the header.
// They call CANConfigParser static methods for string parsing.

uint8_t canConfigParseEngageRules(const JsonArray& steerArray,
                                   CANEngageRule* rules, uint8_t maxRules) {
    uint8_t count = 0;

    for (JsonObject steerObj : steerArray) {
        if (count >= maxRules) break;

        CANEngageRule& rule = rules[count];
        rule = {};  // Zero-initialize

        // Parse CAN ID
        const char* canIdStr = steerObj["canFilterID"];
        if (!canIdStr) continue;
        rule.canId = CANConfigParser::parseHexString(canIdStr);
        rule.isExtendedId = (rule.canId > 0x7FF);

        // Parse label
        const char* label = steerObj["buttonLabel"];
        if (label) {
            strncpy(rule.label, label, sizeof(rule.label) - 1);
        }

        // Parse edge type (default: rising)
        const char* edgeType = steerObj["edgeType"];
        rule.useFallingEdge = (edgeType && strcmp(edgeType, "falling") == 0);

        // Check for multi-byte conditions array (new format)
        if (!steerObj["conditions"].isNull()) {
            JsonArray conditions = steerObj["conditions"];
            rule.conditionCount = 0;
            for (JsonObject cond : conditions) {
                if (rule.conditionCount >= MAX_CONDITIONS_PER_RULE) break;
                CANByteCondition& bc = rule.conditions[rule.conditionCount];
                bc.byteIndex = cond["byte"] | 0;
                bc.mask = cond["mask"] | 0xFF;
                bc.expectedValue = cond["value"] | 0;
                rule.conditionCount++;
            }
        } else {
            // Legacy single-byte format: byte + onStateAND
            rule.conditionCount = 1;
            rule.conditions[0].byteIndex = steerObj["byte"] | 0;
            uint8_t andValue = steerObj["onStateAND"] | 0;
            rule.conditions[0].mask = andValue;
            rule.conditions[0].expectedValue = andValue;
        }

        // Initialize runtime state
        rule.currentState = false;
        rule.previousState = false;
        rule.eventTriggered = false;

        count++;
    }

    return count;
}

bool canConfigParseReceiveConfig(const JsonObject& canConfig, CANReceiveConfig& config) {
    config = {};  // Zero-initialize

    // VReceiveCurve is the CAN ID we listen on for valve/curve data
    const char* receiveIdStr = canConfig["VReceiveCurve"];
    if (!receiveIdStr) {
        // Some configs (like Fendt) use VFilter as the receive ID
        // If VReceiveCurve not present, use first VFilter ID
        const char* vfilterStr = canConfig["VFilter"];
        if (!vfilterStr) return false;

        // Parse first ID from comma-separated list
        config.canId = CANConfigParser::parseHexString(vfilterStr);
    } else {
        config.canId = CANConfigParser::parseHexString(receiveIdStr);
    }

    config.isExtendedId = (config.canId > 0x7FF);

    // Parse curve byte positions
    const char* curveLoHi = canConfig["ReceiveCurveLoHi"];
    if (curveLoHi) {
        CANConfigParser::parseBytePositions(curveLoHi, config.curveLoBytePos, config.curveHiBytePos);
    } else {
        // Default: bytes 0,1
        config.curveLoBytePos = 0;
        config.curveHiBytePos = 1;
    }

    // Parse valve state byte position
    if (!canConfig["ValveState"].isNull()) {
        config.valveStateBytePos = canConfig["ValveState"] | 2;
    } else {
        config.valveStateBytePos = 2;  // Default: byte 2
    }

    // Parse curve scale factor (divisor for raw int16 → degrees, default 100)
    config.curveScale = canConfig["CurveScale"] | 100.0f;

    config.configured = true;
    return true;
}

bool canConfigParseSendConfig(const JsonObject& canConfig, CANSendConfig& config) {
    config = {};  // Zero-initialize

    // VSendCurve is the CAN ID we send steering commands to
    const char* sendIdStr = canConfig["VSendCurve"];
    if (!sendIdStr) return false;
    config.canId = CANConfigParser::parseHexString(sendIdStr);
    config.isExtendedId = (config.canId > 0x7FF);

    // Data length (default 8)
    config.dataLen = canConfig["VDataLen"] | 8;

    // curveMod offset (e.g., Fendt uses -32128)
    config.curveMod = canConfig["curveMod"] | 0;

    // Check for separate steer/no-steer templates (Fendt style)
    const char* steerTemplate = canConfig["SendCurveSteer"];
    const char* noSteerTemplate = canConfig["SendCurveNoSteer"];

    if (steerTemplate && noSteerTemplate) {
        config.hasSeperateTemplates = true;
        uint8_t steerCount = 0, noSteerCount = 0;
        CANConfigParser::parseCommaSeparatedBytes(steerTemplate, config.templateSteer, steerCount);
        CANConfigParser::parseCommaSeparatedBytes(noSteerTemplate, config.templateNoSteer, noSteerCount);

        // For separate templates, curve byte positions may differ
        const char* curveLoHi = canConfig["CurveSteerLoHi"];
        if (curveLoHi) {
            CANConfigParser::parseBytePositions(curveLoHi, config.curveLoBytePos, config.curveHiBytePos);
        } else {
            // Fall back to SendCurveLoHi
            curveLoHi = canConfig["SendCurveLoHi"];
            if (curveLoHi) {
                CANConfigParser::parseBytePositions(curveLoHi, config.curveLoBytePos, config.curveHiBytePos);
            }
        }
    } else {
        // Standard format: single template + intent byte
        config.hasSeperateTemplates = false;
        const char* sendCurve = canConfig["SendCurve"];
        if (sendCurve) {
            uint8_t count = 0;
            CANConfigParser::parseCommaSeparatedBytes(sendCurve, config.templateSteer, count);
            memcpy(config.templateNoSteer, config.templateSteer, 8);
        }

        // Curve byte positions
        const char* curveLoHi = canConfig["SendCurveLoHi"];
        if (curveLoHi) {
            CANConfigParser::parseBytePositions(curveLoHi, config.curveLoBytePos, config.curveHiBytePos);
        }

        // Intent byte (ITS1 = steer active, ITS0 = steer inactive)
        const char* its1 = canConfig["ITS1"];
        const char* its0 = canConfig["ITS0"];
        if (its1) {
            CANConfigParser::parseIntentString(its1, config.intentBytePos, config.intentEnabled);
        }
        if (its0) {
            uint8_t pos = 0;
            CANConfigParser::parseIntentString(its0, pos, config.intentDisabled);
            // intentBytePos should match between ITS1 and ITS0
        }

        // Apply intent values to templates
        if (its1) {
            config.templateSteer[config.intentBytePos] = config.intentEnabled;
        }
        if (its0) {
            config.templateNoSteer[config.intentBytePos] = config.intentDisabled;
        }
    }

    config.configured = true;
    return true;
}

// Parse kickout detection configuration
bool canConfigParseKickoutConfig(const JsonObject& kickoutObj, CANKickoutConfig& config) {
    config = {};  // Zero-initialize (struct has default member initializers)

    // Check if enabled
    config.enabled = kickoutObj["enabled"] | false;
    if (!config.enabled) return false;

    // Parse CAN ID
    const char* canIdStr = kickoutObj["valveStatusMessage"]["canId"];
    if (!canIdStr) return false;
    config.canId = CANConfigParser::parseHexString(canIdStr);

    // Parse status byte position
    config.statusByte = kickoutObj["valveStatusMessage"]["statusByte"] | 2;

    // Parse ready values array
    JsonArray readyVals = kickoutObj["valveStatusMessage"]["readyValues"];
    if (readyVals.isNull()) return false;

    config.readyValueCount = 0;
    for (uint8_t v : readyVals) {
        if (config.readyValueCount < MAX_READY_VALUES) {
            config.readyValues[config.readyValueCount++] = v;
        }
    }

    // Parse other options
    config.onlyWhenEngaged = kickoutObj["onlyWhenEngaged"] | true;
    config.hysteresisMs = kickoutObj["hysteresisMs"] | 100;

    config.configured = true;
    return true;
}

// Parse error messages configuration
uint8_t canConfigParseErrorMessages(const JsonObject& errorObj, CANErrorMessageConfig* configs, uint8_t maxCount) {
    uint8_t count = 0;

    // Get the messages object (it's a JsonObject, not JsonArray)
    JsonObject msgs = errorObj["messages"];
    if (msgs.isNull()) return 0;

    // Iterate over the members of the messages object
    // Each key is the error identifier (e.g., "kickout", "valveNotReady")
    for (JsonPair kv : msgs) {
        if (count >= maxCount) break;

        CANErrorMessageConfig& emc = configs[count];
        // Zero-initialize without memset (struct has non-trivial members due to in-class initialization)
        emc.key[0] = '\0';
        emc.messageTemplate[0] = '\0';
        emc.duration = 3;
        emc.color = 1;
        emc.configured = false;

        // Get the object for this error message
        JsonObject msgObj = kv.value().as<JsonObject>();
        if (!msgObj.isNull()) {
            // Set the key from the JSON member name
            const char* keyName = kv.key().c_str();
            strncpy(emc.key, keyName, sizeof(emc.key) - 1);
            emc.key[sizeof(emc.key) - 1] = '\0';

            // Parse message properties
            if (!msgObj["message"].isNull()) {
                const char* msg = msgObj["message"];
                if (msg) {
                    strncpy(emc.messageTemplate, msg, sizeof(emc.messageTemplate) - 1);
                    emc.messageTemplate[sizeof(emc.messageTemplate) - 1] = '\0';
                }
            }

            if (!msgObj["duration"].isNull()) {
                emc.duration = msgObj["duration"] | 3;
            }

            if (!msgObj["color"].isNull()) {
                emc.color = msgObj["color"] | 1;
            }

            emc.configured = true;
            count++;
        }
    }

    return count;
}
