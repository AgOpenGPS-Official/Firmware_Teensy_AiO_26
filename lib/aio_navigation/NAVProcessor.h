// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef NAV_PROCESSOR_H
#define NAV_PROCESSOR_H

#include "Arduino.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include "QNetworkBase.h"

enum class NavMessageType {
    NONE,
    PANDA,  // Single GPS with/without IMU
    PAOGI   // Dual GPS with/without IMU
};

class NAVProcessor {
private:
    static NAVProcessor* instance;

    // Message buffer
    static constexpr size_t BUFFER_SIZE = 256;
    char messageBuffer[BUFFER_SIZE];

    // Track when we last sent GPS data to AgIO (for hasGPSDataFlow check)
    uint32_t lastGPSMessageTime;

    // Latency monitoring
    bool latencyDisplayEnabled;
    uint32_t latencySum;
    uint32_t latencyCount;
    uint32_t latencyMin;
    uint32_t latencyMax;
    uint32_t lastLatencyReportTime;

    // Private constructor for singleton
    NAVProcessor();

    // Message formatting methods
    NavMessageType selectMessageType();
    bool formatPANDAMessage();
    bool formatPAOGIMessage();

    // Utility methods
    void convertToNMEACoordinates(double decimalDegrees, bool isLongitude,
                                  double& nmeaValue, char& direction);
    uint8_t calculateNMEAChecksum(const char* sentence);
    float convertGPStoUTC(uint16_t gpsWeek, float gpsSeconds);
    void sendMessage(const char* message);

public:
    ~NAVProcessor();

    // Singleton access
    static NAVProcessor* getInstance();
    static void init();

    // Immediate send method - called by GNSSProcessor callback
    void sendImmediately();

    // Status and debugging
    void printStatus();
    uint32_t getLastGPSMessageTime() const { return lastGPSMessageTime; }
    NavMessageType getCurrentMessageType();

    // GPS data flow status - are we sending GPS data to AgIO?
    bool hasGPSDataFlow() const {
        return (millis() - lastGPSMessageTime) < 5000;
    }

    // Latency monitoring
    void toggleLatencyDisplay();
    bool isLatencyDisplayEnabled() const { return latencyDisplayEnabled; }
};

// Global instance declaration
extern NAVProcessor navProcessor;

#endif // NAV_PROCESSOR_H
