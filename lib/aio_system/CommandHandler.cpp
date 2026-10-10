// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "CommandHandler.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SimpleScheduler/SimpleScheduler.h"
#include "SerialManager.h"
#include "NAVProcessor.h"
extern ConfigManager configManager;

// External function declarations
extern void toggleLoopTiming();
extern void toggleProcessTiming();

// Static instance pointer
CommandHandler* CommandHandler::instance = nullptr;

CommandHandler::CommandHandler() {
    loggerPtr = EventLogger::getInstance();
}

CommandHandler::~CommandHandler() {
    instance = nullptr;
}

CommandHandler* CommandHandler::getInstance() {
    if (instance == nullptr) {
        instance = new CommandHandler();
    }
    return instance;
}

void CommandHandler::init() {
    getInstance();  // Create instance if needed
}

void CommandHandler::process() {
    if (!Serial.available()) {
        return;
    }
    
    char cmd = Serial.read();

    // While entering an OTA PIN, all input goes to the PIN prompt
    if (pinEntry != PinEntry::NONE) {
        handlePinChar(cmd);
        return;
    }
    
    // Ignore line ending characters (CR and LF)
    if (cmd == '\r' || cmd == '\n') {
        return;
    }
    
    // Handle command
    handleCommand(cmd);
}

void CommandHandler::handleCommand(char cmd) {
    EventConfig& config = loggerPtr->getConfig();
    
    switch (cmd) {
        case '1':  // Toggle serial output
            loggerPtr->enableSerial(!config.enableSerial);
            Serial.printf("\r\nSerial logging %s\r\n", config.enableSerial ? "ENABLED" : "DISABLED");
            break;
            
        case '2':  // Toggle UDP syslog
            loggerPtr->enableUDP(!config.enableUDP);
            Serial.printf("\r\nUDP syslog %s\r\n", config.enableUDP ? "ENABLED" : "DISABLED");
            break;
            
        case '3':  // Increase serial level
            if (config.serialLevel > 0) {
                config.serialLevel--;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '4':  // Decrease serial level
            if (config.serialLevel < 7) {
                config.serialLevel++;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '5':  // Increase UDP level
            if (config.udpLevel > 0) {
                config.udpLevel--;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '6':  // Decrease UDP level
            if (config.udpLevel < 7) {
                config.udpLevel++;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '7':  // Toggle rate limiting
            loggerPtr->setRateLimitEnabled(!loggerPtr->isRateLimitEnabled());
            break;
            
        case 't':  // Test log messages
        case 'T':
            LOG_INFO(EventSource::USER, "Generating test log messages...");
            LOG_DEBUG(EventSource::USER, "Test DEBUG message");
            LOG_INFO(EventSource::USER, "Test INFO message");
            LOG_NOTICE(EventSource::USER, "Test NOTICE message");
            LOG_WARNING(EventSource::USER, "Test WARNING message");
            LOG_ERROR(EventSource::USER, "Test ERROR message");
            LOG_CRITICAL(EventSource::USER, "Test CRITICAL message");
            LOG_ALERT(EventSource::USER, "Test ALERT message");
            LOG_EMERGENCY(EventSource::USER, "Test EMERGENCY message");
            break;
            
        case 's':  // Show statistics
        case 'S':
            Serial.printf("\r\n\nEvent Statistics:");
            Serial.printf("\r\nTotal events logged: %lu\r\n", loggerPtr->getEventCount());
            break;
            
        case 'r':  // Reset counter
        case 'R':
            loggerPtr->resetEventCount();
            Serial.print("Event counter reset\r\n");
            break;
            
        // QNEthernet doesn't need explicit log level management
        // Removed Mongoose log level option
            
        case 'l':  // Loop timing diagnostics (moved from T to avoid conflict)
        case 'L':
            toggleLoopTiming();
            break;

        case 'p':  // Process timing diagnostics
        case 'P':
            toggleProcessTiming();
            break;
            
        case 'b':  // Buzzer test
        case 'B':
            {
                extern HardwareManager hardwareManager;
                Serial.print("\r\nTesting buzzer...\r\n");
                hardwareManager.performBuzzerTest();
            }
            break;
            
        case 'v':  // Cycle buzzer volume (Quiet -> Loud -> Off -> Quiet)
        case 'V':
            {
                extern ConfigManager configManager;
                uint8_t vol = (configManager.getBuzzerVolume() + 1) % 3;
                configManager.setBuzzerVolume(vol);
                configManager.saveMiscConfig();
                const char* volNames[] = {"QUIET", "LOUD", "OFF"};
                Serial.printf("\r\nBuzzer volume set to: %s\r\n", volNames[vol]);
            }
            break;

        case 'c':  // Show scheduler status
        case 'C':
            {
                extern SimpleScheduler scheduler;
                scheduler.printStatus();
            }
            break;

        case 'g':  // GPS->UDP latency display
        case 'G':
            NAVProcessor::getInstance()->toggleLatencyDisplay();
            break;

        case 'o':  // Set / clear OTA PIN
        case 'O':
            startPinEntry();
            break;

        case '?':
        case 'h':
        case 'H':
            showMenu();
            break;

        case 'm':  // Start buffer Monitoring
        case 'M':
            Serial.printf("\r\nStarting serial buffer monitoring...\r\n");
            {
                extern SerialManager serialManager;
                serialManager.startBufferMonitoring();
            }
            break;

        case 'u':  // View buffer Usage
        case 'U':
            {
                extern SerialManager serialManager;
                serialManager.printBufferUsage();
            }
            break;

        case 'z':  // print scheduler Stats
        case 'Z':
            #ifdef SCHEDULER_TIMING_STATS
            {
                extern SimpleScheduler scheduler;
                scheduler.printStats();
            }
            #else
            Serial.printf("\r\nScheduler timing stats not enabled. Add -D SCHEDULER_TIMING_STATS to build flags.\r\n");
            #endif
            break;

        case 'x':  // reset scheduler stats (X marks the spot to start fresh)
        case 'X':
            #ifdef SCHEDULER_TIMING_STATS
            {
                extern SimpleScheduler scheduler;
                scheduler.resetStats();
                Serial.printf("\r\nScheduler stats reset.\r\n");
            }
            #else
            Serial.printf("\r\nScheduler timing stats not enabled.\r\n");
            #endif
            break;

        default:
            Serial.printf("\r\nUnknown command: '%c'\r\n", cmd);
            break;
    }
}


void CommandHandler::startPinEntry() {
    pinEntry = PinEntry::ENTER;
    pinLen = 0;
    memset(pinBuf, 0, sizeof(pinBuf));
    memset(pinFirst, 0, sizeof(pinFirst));
    Serial.printf("\r\nOTA PIN is currently %s.", configManager.hasOtaPin() ? "SET" : "NOT SET (web firmware updates disabled)");
    Serial.printf("\r\nEnter new PIN (%u-%u printable characters, no spaces), then Enter.",
                  (unsigned)ConfigManager::OTA_PIN_MIN_LEN, (unsigned)ConfigManager::OTA_PIN_MAX_LEN);
    Serial.print("\r\nEmpty line clears the PIN (disables web updates). ESC cancels.\r\nPIN: ");
}

void CommandHandler::handlePinChar(char c) {
    if (c == 0x1B) {  // ESC
        pinEntry = PinEntry::NONE;
        memset(pinBuf, 0, sizeof(pinBuf));
        memset(pinFirst, 0, sizeof(pinFirst));
        Serial.print("\r\nCancelled.\r\n");
        return;
    }
    if (c == 0x08 || c == 0x7F) {  // Backspace
        if (pinLen > 0) {
            pinBuf[--pinLen] = '\0';
            Serial.print("\b \b");
        }
        return;
    }
    if (c != '\r' && c != '\n') {
        if (c >= 0x21 && c <= 0x7E && pinLen < ConfigManager::OTA_PIN_MAX_LEN) {
            pinBuf[pinLen++] = c;
            Serial.print('*');
        }
        return;
    }

    // Enter pressed
    if (pinLen == 0 && pinEntry == PinEntry::CONFIRM) {
        return;  // Ignore the LF of a CR/LF pair
    }
    if (pinEntry == PinEntry::ENTER) {
        if (pinLen == 0) {
            configManager.clearOtaPin();
            Serial.print("\r\nOTA PIN cleared. Web firmware updates are disabled.\r\n");
            pinEntry = PinEntry::NONE;
            return;
        }
        if (pinLen < ConfigManager::OTA_PIN_MIN_LEN) {
            Serial.printf("\r\nPIN too short (minimum %u). Try again.\r\nPIN: ", (unsigned)ConfigManager::OTA_PIN_MIN_LEN);
            pinLen = 0;
            memset(pinBuf, 0, sizeof(pinBuf));
            return;
        }
        memcpy(pinFirst, pinBuf, sizeof(pinFirst));
        pinLen = 0;
        memset(pinBuf, 0, sizeof(pinBuf));
        pinEntry = PinEntry::CONFIRM;
        Serial.print("\r\nConfirm PIN: ");
        return;
    }

    // CONFIRM
    bool same = (strcmp(pinBuf, pinFirst) == 0);
    if (same && configManager.setOtaPin(pinFirst)) {
        Serial.print("\r\nOTA PIN saved. Use it on the System Update page.\r\n");
    } else {
        Serial.print("\r\nPINs did not match. Nothing changed.\r\n");
    }
    memset(pinBuf, 0, sizeof(pinBuf));
    memset(pinFirst, 0, sizeof(pinFirst));
    pinLen = 0;
    pinEntry = PinEntry::NONE;
}

void CommandHandler::showMenu() {
    loggerPtr->printConfig();
    Serial.print("\r\n=== Firmware Controls ===");
    Serial.print("\r\n1 - Toggle serial output");
    Serial.print("\r\n2 - Toggle UDP syslog");
    Serial.print("\r\n3/4 - Decrease/Increase serial level");
    Serial.print("\r\n5/6 - Decrease/Increase UDP level");
    Serial.print("\r\n7 - Toggle rate limiting");
    Serial.print("\r\nT - Generate test messages");
    Serial.print("\r\nS - Show statistics");
    Serial.print("\r\nR - Reset event counter");
    Serial.print("\r\nL - Toggle loop timing diagnostics");
    Serial.print("\r\nP - Toggle process timing diagnostics");
    Serial.print("\r\nB - Test buzzer");
    Serial.print("\r\nV - Toggle buzzer volume (loud/quiet)");
    Serial.print("\r\nO - Set/clear OTA PIN (required for web firmware updates)");
    Serial.print("\r\nC - Show scheduler status");
    Serial.print("\r\nG - Toggle GPS->UDP latency display");
    Serial.print("\r\nM - Start serial buffer monitoring");
    Serial.print("\r\nU - View serial buffer usage");
    Serial.print("\r\nZ - Print scheduler timing stats");
    Serial.print("\r\nX - Reset scheduler timing stats");
    Serial.print("\r\n? - Show this menu");
    Serial.print("\r\n=========================\r\n");
}


