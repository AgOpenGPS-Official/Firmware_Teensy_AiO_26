// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.

// CANSniffer.h - CAN Bus sniffer for debugging tractor CAN communication
#ifndef CAN_SNIFFER_H
#define CAN_SNIFFER_H

#include <Arduino.h>
#include "CANGlobals.h"

class CANSniffer {
private:
    static constexpr size_t SNIF_BUFFER_SIZE = 500;   // Circular buffer size (number of lines)
    static constexpr size_t LINE_BUFFER_SIZE = 128;   // Max text line length

    struct CANLogEntry {
        uint32_t timestamp;           // millis() timestamp
        char line[LINE_BUFFER_SIZE];  // Pre-formatted text line
    };

    CANLogEntry logBuffer[SNIF_BUFFER_SIZE];
    size_t bufferHead = 0;
    size_t bufferTail = 0;
    bool bufferFull = false;

    // Master enable (no overhead when disabled)
    bool masterEnabled = false;
    uint8_t selectedBus = 1;  // CAN1, CAN2, or CAN3

    uint32_t messageCount = 0;
    uint32_t lastClearedTime = 0;  // For relative timestamps

    // Helper to format a single line
    // Format: [HH:MM:SS.mmm] CAN{N} {RX|TX} 0x{ID} [{len}]: {hex bytes}
    void formatLine(char* output, size_t maxSize, const CAN_message_t& msg, uint8_t busNum, bool isTx);

    // Format timestamp as [HH:MM:SS.mmm] relative to last clear
    void formatTimestamp(char* output, size_t maxSize, uint32_t ts);

public:
    CANSniffer() = default;

    void init();

    // Master enable/disable (no overhead when disabled)
    void setMasterEnabled(bool enabled) { masterEnabled = enabled; }
    bool isMasterEnabled() const { return masterEnabled; }

    // Bus selection
    void setSelectedBus(uint8_t bus);
    uint8_t getSelectedBus() const { return selectedBus; }

    // Log a CAN message (only if masterEnabled and bus matches)
    // isTx: true for transmitted messages, false for received
    void logMessage(const CAN_message_t& msg, uint8_t busNum, bool isTx = false);

    // Get formatted text for web interface
    // Returns number of lines, outputs null-terminated text
    // clearAfterRead: clear buffer after reading (for single-client pattern)
    size_t getFormattedText(char* output, size_t maxSize, bool clearAfterRead = true);

    // Clear buffer
    void clear();

    // Check if data available
    bool hasData() const { return bufferHead != bufferTail || bufferFull; }

    // Statistics
    uint32_t getMessageCount() const { return messageCount; }
    uint32_t getBufferUsage() const;  // Returns percentage 0-100
};

// Global instance (accessed from TractorCANDriver and SimpleWebManager)
extern CANSniffer globalCANSniffer;

#endif // CAN_SNIFFER_H
