// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANConfigParser.h - Parsing helpers for CAN configuration
// Note: JSON-dependent parse functions (parseEngageRules, parseReceiveConfig,
// parseSendConfig) are declared in CANConfigParser.cpp only, since they use
// ArduinoJson types and are only called from CANProtocolEngine.cpp.
#ifndef CAN_CONFIG_PARSER_H
#define CAN_CONFIG_PARSER_H

#include <Arduino.h>
#include "CANProtocolEngine.h"

class CANConfigParser {
public:
    // Parse a hex string like "0x0CAD131C" to uint32_t
    static uint32_t parseHexString(const char* str);

    // Parse comma-separated bytes (decimal or hex with "0x" prefix) into array
    static void parseCommaSeparatedBytes(const char* str, uint8_t* out, uint8_t& count);

    // Parse "lo,hi" byte position string like "0,1" into two positions
    static void parseBytePositions(const char* str, uint8_t& lo, uint8_t& hi);

    // Parse "bytePos,value" intent string like "2,253"
    static void parseIntentString(const char* str, uint8_t& bytePos, uint8_t& value);

    // Parse VFilter comma-separated hex IDs into filter array
    static uint8_t parseFilterIds(const char* vfilterStr, uint32_t* ids, uint8_t maxIds);

    // JSON-dependent functions (declared in .cpp, called only from CANProtocolEngine.cpp):
    //   parseEngageRules(JsonArray&, CANEngageRule*, uint8_t) -> uint8_t
    //   parseReceiveConfig(JsonObject&, CANReceiveConfig&) -> bool
    //   parseSendConfig(JsonObject&, CANSendConfig&) -> bool
    //   parseKickoutConfig(JsonObject&, CANKickoutConfig&) -> bool
    //   parseErrorMessages(JsonObject&, CANErrorMessageConfig*, uint8_t) -> uint8_t
};

#endif // CAN_CONFIG_PARSER_H
