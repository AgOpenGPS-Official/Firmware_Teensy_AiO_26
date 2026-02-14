// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.

#include "CANSniffer.h"

// Global instance
CANSniffer globalCANSniffer;

void CANSniffer::init() {
    // Initialize buffer state
    bufferHead = 0;
    bufferTail = 0;
    bufferFull = false;
    masterEnabled = false;
    selectedBus = 1;
    messageCount = 0;
    lastClearedTime = millis();
}

void CANSniffer::setSelectedBus(uint8_t bus) {
    selectedBus = constrain(bus, 1, 3);
}

void CANSniffer::formatTimestamp(char* output, size_t maxSize, uint32_t ts) {
    uint32_t elapsed = ts - lastClearedTime;
    uint32_t millisecs = elapsed % 1000;
    uint32_t seconds = (elapsed / 1000) % 60;
    uint32_t minutes = (elapsed / 60000) % 60;
    uint32_t hours = (elapsed / 3600000);

    snprintf(output, maxSize, "[%02lu:%02lu:%02lu.%03lu]",
             hours, minutes, seconds, millisecs);
}

void CANSniffer::formatLine(char* output, size_t maxSize, const CAN_message_t& msg, uint8_t busNum, bool isTx) {
    char tsBuf[16];
    formatTimestamp(tsBuf, sizeof(tsBuf), millis());

    // Format CAN ID
    char idBuf[12];
    if (msg.flags.extended) {
        snprintf(idBuf, sizeof(idBuf), "0x%08X", msg.id);
    } else {
        snprintf(idBuf, sizeof(idBuf), "0x%03X", (unsigned int)(msg.id & 0x7FF));
    }

    // Format data bytes
    char dataBuf[32];
    int dataPos = 0;
    for (uint8_t i = 0; i < msg.len && i < 8; i++) {
        dataPos += snprintf(dataBuf + dataPos, sizeof(dataBuf) - dataPos,
                           "%02X ", msg.buf[i]);
    }
    if (msg.len > 0 && dataPos > 0) {
        dataBuf[dataPos - 1] = '\0';  // Remove trailing space
    } else {
        dataBuf[0] = '\0';
    }

    // Full line: [HH:MM:SS.mmm] CAN{N} {RX|TX} 0x{ID} [{len}]: {hex bytes}
    snprintf(output, maxSize, "%s CAN%d %s %s [%d]: %s",
             tsBuf, busNum, isTx ? "TX" : "RX", idBuf, msg.len, dataBuf);
}

void CANSniffer::logMessage(const CAN_message_t& msg, uint8_t busNum, bool isTx) {
    // Fast exit if disabled (minimal overhead)
    if (!masterEnabled) {
        return;
    }

    // Only log the selected bus
    if (busNum != selectedBus) {
        return;
    }

    // Format the line directly into the buffer entry
    CANLogEntry& entry = logBuffer[bufferHead];
    entry.timestamp = millis();
    formatLine(entry.line, sizeof(entry.line), msg, busNum, isTx);

    // Advance buffer head
    bufferHead++;
    if (bufferHead >= SNIF_BUFFER_SIZE) {
        bufferHead = 0;
    }

    // Check for buffer full condition
    if (bufferHead == bufferTail) {
        bufferFull = true;
        // When full, move tail to oldest entry (overwrite)
        bufferTail++;
        if (bufferTail >= SNIF_BUFFER_SIZE) {
            bufferTail = 0;
        }
    } else {
        bufferFull = false;
    }

    messageCount++;
}

size_t CANSniffer::getFormattedText(char* output, size_t maxSize, bool clearAfterRead) {
    if (maxSize == 0 || !hasData()) {
        if (output && maxSize > 0) {
            output[0] = '\0';
        }
        return 0;
    }

    size_t writePos = 0;
    size_t lineCount = 0;

    // Iterate through buffer from tail to head
    size_t idx = bufferTail;
    while (idx != bufferHead) {
        const CANLogEntry& entry = logBuffer[idx];

        // Calculate remaining space
        size_t remaining = maxSize - writePos;
        if (remaining < 2) break;  // Need at least space for newline + null

        // Copy line (truncate if necessary)
        size_t lineLen = strlen(entry.line);
        if (lineLen > remaining - 2) {
            lineLen = remaining - 2;
        }

        if (lineLen > 0) {
            memcpy(output + writePos, entry.line, lineLen);
            writePos += lineLen;
        }

        // Add newline
        output[writePos++] = '\n';
        lineCount++;

        // Advance index
        idx++;
        if (idx >= SNIF_BUFFER_SIZE) {
            idx = 0;
        }
    }

    // Null terminate
    output[writePos] = '\0';

    // Clear after read if requested (for single-client pattern)
    if (clearAfterRead) {
        clear();
    }

    return lineCount;
}

void CANSniffer::clear() {
    bufferHead = 0;
    bufferTail = 0;
    bufferFull = false;
    messageCount = 0;
    lastClearedTime = millis();
}

uint32_t CANSniffer::getBufferUsage() const {
    if (bufferFull) {
        return 100;
    }

    if (bufferHead >= bufferTail) {
        return (bufferHead - bufferTail) * 100 / SNIF_BUFFER_SIZE;
    } else {
        return (SNIF_BUFFER_SIZE - bufferTail + bufferHead) * 100 / SNIF_BUFFER_SIZE;
    }
}
