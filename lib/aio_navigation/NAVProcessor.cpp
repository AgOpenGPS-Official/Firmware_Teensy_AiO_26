// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "NAVProcessor.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "EventLogger.h"
#include "ConfigManager.h"
#include "NMEAMessageBuilder.h"

// External processor instances from main.cpp
extern GNSSProcessor gnssProcessor;
extern IMUProcessor imuProcessor;

// External UDP send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Static instance
NAVProcessor* NAVProcessor::instance = nullptr;

NAVProcessor::NAVProcessor() {
    instance = this;

    // Initialize timing
    lastGPSMessageTime = 0;

    // Initialize latency monitoring
    latencyDisplayEnabled = false;
    latencySum = 0;
    latencyCount = 0;
    latencyMin = UINT32_MAX;
    latencyMax = 0;
    lastLatencyReportTime = 0;

    // Clear message buffer
    memset(messageBuffer, 0, BUFFER_SIZE);

    LOG_INFO(EventSource::GNSS, "NAVProcessor initialized");
}

NAVProcessor::~NAVProcessor() {
    instance = nullptr;
}

NAVProcessor* NAVProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new NAVProcessor();
    }
    return instance;
}

void NAVProcessor::init() {
    if (instance == nullptr) {
        instance = new NAVProcessor();
    }
}

NavMessageType NAVProcessor::selectMessageType() {
    const auto& gnssData = gnssProcessor.getData();

    // For dual/INS systems, send PAOGI even without fix (for INS_ALIGNING state)
    if (gnssData.hasDualHeading || gnssData.hasINS) {
        return NavMessageType::PAOGI;
    }

    // For single GPS, require fix
    if (!gnssProcessor.hasFix()) {
        return NavMessageType::NONE;
    }

    return NavMessageType::PANDA;
}

void NAVProcessor::convertToNMEACoordinates(double decimalDegrees, bool isLongitude,
                                           double& nmeaValue, char& direction) {
    // Determine direction
    if (isLongitude) {
        direction = (decimalDegrees < 0) ? 'W' : 'E';
    } else {
        direction = (decimalDegrees < 0) ? 'S' : 'N';
    }

    // Work with absolute value
    double absDegrees = fabs(decimalDegrees);

    // Extract degrees and minutes
    int degrees = (int)absDegrees;
    double minutes = (absDegrees - degrees) * 60.0;

    // Format as DDDMM.MMMMM or DDMM.MMMMM
    nmeaValue = degrees * 100.0 + minutes;
}

uint8_t NAVProcessor::calculateNMEAChecksum(const char* sentence) {
    uint8_t checksum = 0;

    // Skip the $ and calculate XOR up to * or end
    const char* p = sentence + 1;
    while (*p && *p != '*') {
        checksum ^= *p++;
    }

    return checksum;
}

float NAVProcessor::convertGPStoUTC(uint16_t gpsWeek, float gpsSeconds) {
    // GPS epoch (January 6, 1980) to Unix epoch (January 1, 1970) offset
    const uint32_t GPS_EPOCH_OFFSET = 315964800UL;

    // Current leap seconds as of 2024
    const uint8_t LEAP_SECONDS = 18;

    // Calculate total seconds since GPS epoch
    uint32_t totalGPSSeconds = (uint32_t)gpsWeek * 7 * 24 * 60 * 60 + (uint32_t)gpsSeconds;

    // Convert to Unix time and adjust for leap seconds
    uint32_t unixTime = GPS_EPOCH_OFFSET + totalGPSSeconds - LEAP_SECONDS;

    // Extract UTC time components
    uint32_t secondsToday = unixTime % (24 * 60 * 60);
    uint8_t hours = secondsToday / 3600;
    uint8_t minutes = (secondsToday % 3600) / 60;
    uint8_t seconds = secondsToday % 60;

    // Get milliseconds from the fractional part of gpsSeconds
    float fractionalSeconds = gpsSeconds - (uint32_t)gpsSeconds;

    // Return as HHMMSS.S format
    return hours * 10000.0f + minutes * 100.0f + seconds + fractionalSeconds;
}

bool NAVProcessor::formatPANDAMessage() {
    if (!gnssProcessor.hasGPS()) {
        return false;
    }

    const auto& gnssData = gnssProcessor.getData();

    // Use cached NMEA coordinates - no conversion needed
    double latNMEA = gnssData.latitudeNMEA;
    double lonNMEA = gnssData.longitudeNMEA;
    char latDir = gnssData.latDir;
    char lonDir = gnssData.lonDir;

    // Get IMU data if available
    char imuHeading[10] = "65535";  // Default "no IMU" value
    char imuRoll[10] = "0";
    char imuPitch[10] = "0";
    char imuYawRate[10] = "0";

    if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        snprintf(imuHeading, sizeof(imuHeading), "%d", (int)(imuData.heading * 10.0));
        snprintf(imuRoll, sizeof(imuRoll), "%d", (int)round(imuData.roll));
        snprintf(imuPitch, sizeof(imuPitch), "%d", (int)round(imuData.pitch));
        snprintf(imuYawRate, sizeof(imuYawRate), "%.2f", imuData.yawRate);
    }

    // Format time (HHMMSS.S)
    float timeFloat = gnssData.fixTime + gnssData.fixTimeFractional;

    // Build PANDA message using MessageBuilder
    NMEAMessageBuilder builder(messageBuffer);

    builder.addString("$PANDA");
    builder.addComma();
    builder.addFloat(timeFloat, 1);
    builder.addComma();
    builder.addLatitude(latNMEA);
    builder.addComma();
    builder.addChar(latDir);
    builder.addComma();
    builder.addLongitude(lonNMEA);
    builder.addComma();
    builder.addChar(lonDir);
    builder.addComma();
    builder.addInt(gnssData.fixQuality);
    builder.addComma();
    builder.addInt(gnssData.numSatellites);
    builder.addComma();
    builder.addFloat(gnssData.hdop, 1);
    builder.addComma();
    builder.addFloat(gnssData.altitude, 3);
    builder.addComma();
    builder.addFloat((float)gnssData.ageDGPS, 1);
    builder.addComma();
    builder.addFloat(gnssData.speedKnots, 3);
    builder.addComma();
    builder.addString(imuHeading);
    builder.addComma();
    builder.addString(imuRoll);
    builder.addComma();
    builder.addString(imuPitch);
    builder.addComma();
    builder.addString(imuYawRate);
    builder.addChecksum();
    builder.terminate();

    return true;
}

bool NAVProcessor::formatPAOGIMessage() {
    const auto& gnssData = gnssProcessor.getData();

    if (!gnssData.hasDualHeading) {
        return false;
    }

    // Use cached NMEA coordinates - no conversion needed
    double latNMEA = gnssData.latitudeNMEA;
    double lonNMEA = gnssData.longitudeNMEA;
    char latDir = gnssData.latDir;
    char lonDir = gnssData.lonDir;

    // Get IMU data if available (for pitch and yaw rate)
    int16_t pitch = 0;
    float yawRate = 0.0;

    // Prefer INS pitch if available (from UM981)
    if (gnssData.hasINS) {
        pitch = (int16_t)round(gnssData.insPitch);
    } else if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        pitch = (int16_t)round(imuData.pitch);
        yawRate = imuData.yawRate;
    }

    // Use dual GPS roll (from KSXT pitch field)
    float roll = gnssData.dualRoll;

    // Format time - use UTC from GPS week/seconds if available
    float timeFloat;
    if (gnssData.gpsWeek > 0 && gnssData.gpsSeconds > 0) {
        timeFloat = convertGPStoUTC(gnssData.gpsWeek, gnssData.gpsSeconds);
    } else {
        timeFloat = gnssData.fixTime + gnssData.fixTimeFractional;
    }

    // Build PAOGI message using MessageBuilder
    NMEAMessageBuilder builder(messageBuffer);

    builder.addString("$PAOGI");
    builder.addComma();
    builder.addFloat(timeFloat, 1);
    builder.addComma();
    builder.addLatitude(latNMEA);
    builder.addComma();
    builder.addChar(latDir);
    builder.addComma();
    builder.addLongitude(lonNMEA);
    builder.addComma();
    builder.addChar(lonDir);
    builder.addComma();
    builder.addInt(gnssData.fixQuality);
    builder.addComma();
    builder.addInt(gnssData.numSatellites);
    builder.addComma();
    builder.addFloat(gnssData.hdop, 1);
    builder.addComma();
    builder.addFloat(gnssData.altitude, 3);
    builder.addComma();
    builder.addFloat((float)gnssData.ageDGPS, 1);
    builder.addComma();
    builder.addFloat(gnssData.speedKnots, 3);
    builder.addComma();
    builder.addFloat(gnssData.dualHeading, 1);
    builder.addComma();
    builder.addFloat(roll, 2);
    builder.addComma();
    builder.addInt(pitch);
    builder.addComma();
    builder.addFloat(yawRate, 2);
    builder.addChecksum();
    builder.terminate();

    return true;
}

void NAVProcessor::sendMessage(const char* message) {
    // NMEA messages must end with CR+LF
    char buffer[BUFFER_SIZE + 3];
    snprintf(buffer, sizeof(buffer), "%s\r\n", message);

    // Send via UDP to AgIO
    sendUDPbytes((uint8_t*)buffer, strlen(buffer));
}

void NAVProcessor::sendImmediately() {
    // Start timing for latency measurement
    uint32_t startMicros = micros();

    // Check if UDP passthrough is enabled - if so, don't send PANDA/PAOGI
    extern ConfigManager configManager;
    if (configManager.getGPSPassThrough()) {
        return;
    }

    // Select and format appropriate message type
    NavMessageType msgType = selectMessageType();
    if (msgType == NavMessageType::NONE) {
        return;
    }

    bool success = false;

    switch (msgType) {
        case NavMessageType::PANDA:
            success = formatPANDAMessage();
            break;
        case NavMessageType::PAOGI:
            success = formatPAOGIMessage();
            break;
        default:
            break;
    }

    if (success) {
        sendMessage(messageBuffer);
        lastGPSMessageTime = millis();

        // Measure and track latency
        if (latencyDisplayEnabled) {
            uint32_t latencyMicros = micros() - startMicros;
            latencySum += latencyMicros;
            latencyCount++;
            if (latencyMicros < latencyMin) latencyMin = latencyMicros;
            if (latencyMicros > latencyMax) latencyMax = latencyMicros;

            // Report every second
            uint32_t now = millis();
            if (now - lastLatencyReportTime >= 1000) {
                if (latencyCount > 0) {
                    uint32_t avgLatency = latencySum / latencyCount;
                    Serial.printf("[GPS->UDP] %lu msgs, latency: avg=%luus, min=%luus, max=%luus\r\n",
                                  latencyCount, avgLatency, latencyMin, latencyMax);
                }
                // Reset for next period
                latencySum = 0;
                latencyCount = 0;
                latencyMin = UINT32_MAX;
                latencyMax = 0;
                lastLatencyReportTime = now;
            }
        }
    }
}

NavMessageType NAVProcessor::getCurrentMessageType() {
    return selectMessageType();
}

void NAVProcessor::printStatus() {
    LOG_INFO(EventSource::GNSS, "=== NAVProcessor Status ===");

    NavMessageType currentType = getCurrentMessageType();
    LOG_INFO(EventSource::GNSS, "Current mode: %s",
        currentType == NavMessageType::PANDA ? "PANDA (Single GPS)" :
        currentType == NavMessageType::PAOGI ? "PAOGI (Dual GPS)" : "NONE");

    if (lastGPSMessageTime > 0) {
        LOG_INFO(EventSource::GNSS, "Time since last GPS message: %lu ms",
            millis() - lastGPSMessageTime);
    }

    // Show data sources
    LOG_INFO(EventSource::GNSS, "Data sources:");
    if (gnssProcessor.isValid()) {
        const auto& gnssData = gnssProcessor.getData();
        LOG_INFO(EventSource::GNSS, "  GPS: Valid (Fix=%d, Sats=%d)",
            gnssData.fixQuality, gnssData.numSatellites);
        if (gnssData.hasDualHeading) {
            LOG_INFO(EventSource::GNSS, "  Dual GPS: Active (Quality=%d)",
                gnssData.headingQuality);
        }
    } else {
        LOG_INFO(EventSource::GNSS, "  GPS: No valid fix");
    }

    if (imuProcessor.hasValidData()) {
        LOG_INFO(EventSource::GNSS, "  IMU: %s connected", imuProcessor.getIMUTypeName());
    } else {
        LOG_INFO(EventSource::GNSS, "  IMU: Not detected");
    }
}

void NAVProcessor::toggleLatencyDisplay() {
    latencyDisplayEnabled = !latencyDisplayEnabled;

    if (latencyDisplayEnabled) {
        // Reset stats when enabling
        latencySum = 0;
        latencyCount = 0;
        latencyMin = UINT32_MAX;
        latencyMax = 0;
        lastLatencyReportTime = millis();
        Serial.println("\r\nGPS->UDP latency display ENABLED (reports every second)");
    } else {
        Serial.println("\r\nGPS->UDP latency display DISABLED");
    }
}
