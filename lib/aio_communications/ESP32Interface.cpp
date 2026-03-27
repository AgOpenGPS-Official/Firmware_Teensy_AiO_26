// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "ESP32Interface.h"
#include "SerialManager.h"
#include "QNEthernetUDPHandler.h"
#include "EventLogger.h"

// proxyBuffer in RAM2 (Teensy 4.1 has 512KB RAM2 for DMA/data, free for malloc/new)
DMAMEM uint8_t ESP32Interface::proxyBuffer[ESP32Interface::PROXY_BUFFER_SIZE + 1];  // +1 guard byte

// Global instance
ESP32Interface esp32Interface;

// Initialize the interface
void ESP32Interface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "ESP32 interface initialized on Serial2 (460800 baud)");
    // Already logged with LOG_INFO above
    
    // Clear receive buffer
    rxBufferIndex = 0;
    memset(rxBuffer, 0, RX_BUFFER_SIZE);
}

// Main processing loop - called from main.cpp
void ESP32Interface::process() {
    // Debug: Show we're running
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {  // Every 10 seconds
        LOG_DEBUG(EventSource::SYSTEM, "ESP32Interface: Running, detected=%s, Serial2 available=%d", 
                      esp32Detected ? "YES" : "NO", SerialESP32.available());
        lastDebugTime = millis();
    }
    
    // Process any incoming serial data
    processIncomingData();
    
    // Check for timeout - if we haven't heard hello in a while
    if (esp32Detected && (millis() - lastHelloTime > HELLO_TIMEOUT_MS)) {
        esp32Detected = false;
        LOG_WARNING(EventSource::SYSTEM, "ESP32 connection lost (hello timeout)");
        LOG_DEBUG(EventSource::SYSTEM, "ESP32 hello timeout - last hello was %lu ms ago", 
                      millis() - lastHelloTime);
    }
}

// Send data to ESP32 (called by UDP handler)
void ESP32Interface::sendToESP32(const uint8_t* data, size_t length) {
    if (!esp32Detected) {
        return;  // Don't send if ESP32 not detected
    }
    
    // Log first few PGNs for debugging
    static int pgnCount = 0;
    if (pgnCount < 20 && length >= 5) {
        // Calculate CRC for debug logging
        uint8_t calcCrc = 0;
        if (length >= 6) {
            uint16_t crcSum = 0;
            for (size_t i = 2; i < length - 1; i++) {
                crcSum += data[i];
            }
            calcCrc = (uint8_t)(crcSum & 0xFF);
        }
        LOG_DEBUG(EventSource::SYSTEM, "ESP32 TX: PGN=%d, len=%zu, CRC: calc=%02X pkt=%02X", 
                  data[3], length, calcCrc, data[length-1]);
        pgnCount++;
    }
    
    // Send raw bytes to ESP32
    SerialESP32.write(data, length);
}

// Process incoming data from ESP32
// Normal operation: reads all available bytes into buffer, then processes PGNs and hello.
// During a proxy request: proxyRequest() calls checkForProxyResponse() directly and
// processIncomingData() is not called, so 0xFE 0x02 frames are never treated as PGNs.
void ESP32Interface::processIncomingData() {
    // During proxy request, checkForProxyResponse() handles all UART reading
    if (proxyPending) return;

    static bool firstByte = true;

    // Read all available bytes into rxBuffer
    while (SerialESP32.available() && rxBufferIndex < RX_BUFFER_SIZE) {
        rxBuffer[rxBufferIndex++] = (uint8_t)SerialESP32.read();
        if (firstByte) {
            LOG_DEBUG(EventSource::SYSTEM, "ESP32Interface: Receiving data from ESP32!");
            firstByte = false;
        }
    }

    if (rxBufferIndex == 0) return;

    // Skip 0xFE frames (proxy response during unexpected state - just discard)
    if (rxBuffer[0] == 0xFE) {
        if (rxBufferIndex >= 2 && rxBuffer[1] == 0x02 && rxBufferIndex >= 4) {
            uint16_t bodyLen = ((uint16_t)rxBuffer[2] << 8) | rxBuffer[3];
            if (bodyLen > 48000) bodyLen = 0;
            size_t frameLen = 4 + bodyLen;
            if (rxBufferIndex >= frameLen) {
                LOG_WARNING(EventSource::SYSTEM, "ESP32 RX: Unexpected 0xFE 0x02 frame discarded");
                size_t remaining = rxBufferIndex - frameLen;
                if (remaining > 0) memmove(rxBuffer, &rxBuffer[frameLen], remaining);
                rxBufferIndex = remaining;
            }
        }
        return;
    }

    // Check for hello message
    checkForHello();

    // Check for complete PGN message
    // PGN format: [0x80][0x81][Source][PGN][Length][Data...][CRC]
    if (rxBufferIndex >= 7) {
        bool foundPGN = false;
        size_t pgnStart = 0;
        
        for (size_t i = 0; i <= rxBufferIndex - 7; i++) {
            if (rxBuffer[i] == 0x80 && rxBuffer[i + 1] == 0x81) {
                pgnStart = i;
                uint8_t dataLength = rxBuffer[i + 4];
                size_t totalLength = 5 + dataLength + 1;
                
                if ((pgnStart + totalLength) <= rxBufferIndex) {
                    foundPGN = true;
                    uint8_t* pgnData = &rxBuffer[pgnStart];
                    uint8_t source = rxBuffer[pgnStart + 2];
                    uint8_t pgn = rxBuffer[pgnStart + 3];
                    LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: PGN=%d, source=%d, len=%zu -> UDP9999",
                              pgn, source, totalLength);
                    QNEthernetUDPHandler::sendUDP9999Packet(pgnData, totalLength);
                    size_t remaining = rxBufferIndex - (pgnStart + totalLength);
                    if (remaining > 0) memmove(rxBuffer, &rxBuffer[pgnStart + totalLength], remaining);
                    rxBufferIndex = remaining;
                    break;
                } else {
                    // Incomplete message - wait for more bytes (normal for serial)
                    static uint32_t incompleteStartTime = 0;
                    static size_t lastIncompleteSize = 0;
                    if (rxBufferIndex != lastIncompleteSize) {
                        incompleteStartTime = millis();
                        lastIncompleteSize = rxBufferIndex;
                    }
                    if (millis() - incompleteStartTime > 50) {
                        static uint32_t lastIncompleteLog = 0;
                        if (millis() - lastIncompleteLog > 1000) {
                            LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: Incomplete PGN at %zu, need %zu bytes, have %zu",
                                      pgnStart, totalLength, rxBufferIndex - pgnStart);
                            lastIncompleteLog = millis();
                        }
                    }
                }
            }
        }

        if (!foundPGN && rxBufferIndex > RX_BUFFER_SIZE - 100) {
            LOG_WARNING(EventSource::SYSTEM, "ESP32 RX: Buffer full, clearing old data (%zu bytes)", rxBufferIndex);
            memmove(rxBuffer, &rxBuffer[rxBufferIndex - 100], 100);
            rxBufferIndex = 100;
        }

        // Clear stuck partial message after 100ms
        static uint32_t partialMessageTime = 0;
        if (foundPGN) {
            partialMessageTime = 0;
        } else if (rxBufferIndex > 0 && rxBuffer[0] == 0x80) {
            if (partialMessageTime == 0) partialMessageTime = millis();
            if (millis() - partialMessageTime > 100) {
                LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: Clearing partial message after timeout (%zu bytes)", rxBufferIndex);
                rxBufferIndex = 0;
                partialMessageTime = 0;
            }
        } else {
            partialMessageTime = 0;
        }
    }
}

// Check buffer for ESP32 hello message
void ESP32Interface::checkForHello() {
    const char* helloMsg = "ESP32-hello";
    size_t helloLen = strlen(helloMsg);
    
    // Debug: Show buffer contents periodically (disabled - too noisy with constant PGN traffic)
    /*
    static uint32_t lastBufferDebug = 0;
    if (rxBufferIndex > 0 && millis() - lastBufferDebug > 2000) {
        Serial.printf("ESP32 RX Buffer (%d bytes): ", rxBufferIndex);
        for (size_t i = 0; i < rxBufferIndex && i < 20; i++) {
            if (rxBuffer[i] >= 32 && rxBuffer[i] <= 126) {
                Serial.printf("%c", rxBuffer[i]);
            } else {
                Serial.printf("[%02X]", rxBuffer[i]);
            }
        }
        if (rxBufferIndex > 20) Serial.print("...");
        Serial.println();
        lastBufferDebug = millis();
    }
    */
    
    // Look for hello message in buffer
    if (rxBufferIndex >= helloLen) {
        for (size_t i = 0; i <= rxBufferIndex - helloLen; i++) {
            if (memcmp(&rxBuffer[i], helloMsg, helloLen) == 0) {
                // Found hello message
                if (!esp32Detected) {
                    esp32Detected = true;
                    LOG_INFO(EventSource::SYSTEM, "ESP32 detected and connected");
                    LOG_INFO(EventSource::SYSTEM, "ESP32 will now receive PGNs from UDP port 8888");
                    // Already logged with LOG_INFO above
                } else {
                    // Already detected, just update the time
                    static uint32_t lastHelloLog = 0;
                    if (millis() - lastHelloLog > 30000) {  // Log every 30 seconds
                        LOG_DEBUG(EventSource::SYSTEM, "ESP32: Hello received, connection maintained");
                        lastHelloLog = millis();
                    }
                }
                lastHelloTime = millis();
                
                // Remove hello message from buffer
                size_t remaining = rxBufferIndex - (i + helloLen);
                if (remaining > 0) {
                    memmove(&rxBuffer[i], &rxBuffer[i + helloLen], remaining);
                }
                rxBufferIndex -= helloLen;
                
                break;
            }
        }
    }
}

// Print status information
void ESP32Interface::printStatus() {
    LOG_INFO(EventSource::SYSTEM, "ESP32 Interface Status: Detected=%s", esp32Detected ? "Yes" : "No");
    
    if (esp32Detected) {
        LOG_INFO(EventSource::SYSTEM, "  Last hello: %lu ms ago", millis() - lastHelloTime);
    }
    
    LOG_INFO(EventSource::SYSTEM, "  RX buffer: %zu bytes", rxBufferIndex);
}

// -----------------------------------------------------------------------
// Proxy: Forward HTTP request to WiFi module via ESP32 UART
// Protocol: 0xFE 0x01 [len_hi][len_lo][url_without_http://]
// Response: 0xFE 0x02 [len_hi][len_lo][body...]
// -----------------------------------------------------------------------
bool ESP32Interface::proxyRequest(const String& url, String& responseBody) {
    // Note: we attempt the proxy even if esp32Detected==false (hello may be missed
    // but UART link might still work). Real failure shows as timeout.

    // Extract host+path from url (remove "http://")
    String target = url;
    if (target.startsWith("http://")) target = target.substring(7);

    uint16_t urlLen = (uint16_t)target.length();
    if (urlLen == 0 || urlLen > 400) {
        responseBody = "<h2>400 - Ungueltige URL</h2>";
        return false;
    }

    // Flush any stale bytes that accumulated in the UART RX hardware buffer
    // from normal PGN traffic before the proxy request. Without this, leftover
    // bytes appear as garbage *before* the 0xFE 0x02 response header, causing
    // the Teensy to wait for a body that never fully arrives.
    while (SerialESP32.available()) SerialESP32.read();

    // Set proxyPending BEFORE sending so no bytes are missed
    proxyResponseReady = false;
    proxyResponseBody = "";
    proxyPending = true;
    proxyRequestTime = millis();
    proxyBufferIndex = 0;  // clear proxy receive buffer

    // Send proxy request frame: 0xFE 0x01 [len_hi][len_lo][url]
    uint8_t header[4] = { 0xFE, 0x01,
        (uint8_t)(urlLen >> 8), (uint8_t)(urlLen & 0xFF) };
    LOG_INFO(EventSource::NETWORK, "Proxy request: %s", target.c_str());
    SerialESP32.write(header, 4);
    SerialESP32.write((const uint8_t*)target.c_str(), urlLen);
    SerialESP32.flush();

    uint32_t deadline = millis() + PROXY_TIMEOUT_MS;
    while (millis() < deadline) {
        // Process incoming bytes looking for 0xFE 0x02 response
        checkForProxyResponse();
        if (proxyResponseReady) {
            responseBody = proxyResponseBody;
            proxyPending = false;
            return true;
        }
        yield();  // Allow interrupts/UART ISR without blocking byte reception
    }

    proxyPending = false;
    responseBody = "<h2>504 - Timeout: Modul antwortet nicht</h2><p>" + url + "</p>";
    LOG_WARNING(EventSource::SYSTEM, "ESP32 Proxy timeout for: %s", target.c_str());
    return false;
}

// Check receive buffer for proxy response frame: 0xFE 0x02 [len_hi][len_lo][body]
void ESP32Interface::checkForProxyResponse() {
    // Bulk-read all available bytes into the dedicated proxy buffer (much faster
    // than byte-by-byte which caused ~1 byte/call = thousands of iterations for
    // a 40 KB page, filling the timeout window without completing reception).
    if (proxyBufferIndex < PROXY_BUFFER_SIZE) {
        int avail = SerialESP32.available();
        if (avail > 0) {
            size_t space = PROXY_BUFFER_SIZE - proxyBufferIndex;
            size_t toRead = (size_t)avail < space ? (size_t)avail : space;
            SerialESP32.readBytes(reinterpret_cast<char*>(proxyBuffer + proxyBufferIndex), toRead);
            proxyBufferIndex += toRead;
        }
    }

    if (!proxyPending) return;
    if (proxyBufferIndex < 4) return;

    // Search for 0xFE 0x02 header (skip any garbage bytes before the frame)
    for (size_t i = 0; i <= proxyBufferIndex - 4; i++) {
        if (proxyBuffer[i] == 0xFE && proxyBuffer[i + 1] == 0x02) {
            uint16_t bodyLen = ((uint16_t)proxyBuffer[i + 2] << 8) | proxyBuffer[i + 3];
            if (bodyLen > PROXY_BUFFER_SIZE) bodyLen = PROXY_BUFFER_SIZE;

            // Wait until full body is in buffer
            if ((i + 4 + (size_t)bodyLen) > proxyBufferIndex) return;

            // Extract body: Teensy WString has no String(char*,len).
            // The proxyBuffer has PROXY_BUFFER_SIZE bytes. We ensure the byte at
            // PROXY_BUFFER_SIZE-1 is always a safe write target by declaring the
            // buffer one byte larger than PROXY_BUFFER_SIZE in the header.
            // Here we use a guaranteed-safe write position: proxyBuffer[i+4+bodyLen]
            // which is always < PROXY_BUFFER_SIZE (the guard check above ensures
            // i+4+bodyLen <= proxyBufferIndex <= PROXY_BUFFER_SIZE-1, so the byte
            // at i+4+bodyLen is within the allocated array and can be temporarily zeroed).
            uint8_t savedByte = proxyBuffer[i + 4 + bodyLen];
            proxyBuffer[i + 4 + bodyLen] = 0;  // temporary null terminator
            proxyResponseBody = reinterpret_cast<const char*>(proxyBuffer + i + 4);
            proxyBuffer[i + 4 + bodyLen] = savedByte;  // restore immediately
            proxyResponseReady = true;

            // Clear proxy buffer
            proxyBufferIndex = 0;

            LOG_INFO(EventSource::NETWORK, "ESP32 Proxy RX: %u bytes body", bodyLen);
            return;
        }
    }

    // Buffer full but no valid frame found - clear it
    if (proxyBufferIndex >= PROXY_BUFFER_SIZE) {
        LOG_WARNING(EventSource::SYSTEM, "ESP32 Proxy: buffer full, no 0xFE02 frame found - clearing");
        proxyBufferIndex = 0;
    }
}