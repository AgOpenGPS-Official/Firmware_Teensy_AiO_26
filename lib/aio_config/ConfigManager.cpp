// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "ConfigManager.h"
#include "EventLogger.h"
#include "EEPROMLayout.h"

// Use shared EEPROM version from EEPROMLayout.h
#define CURRENT_EE_VERSION EEPROM_VERSION

// Direct serial logging for early initialization
#define EARLY_LOG(msg) Serial.print("\r\n[CONFIG] " msg "\r\n")

// Static instance pointer
ConfigManager *ConfigManager::instance = nullptr;

ConfigManager::ConfigManager()
{
    instance = this;
    initialized = false;
    // Defer actual initialization until Serial is ready
}

void ConfigManager::init()
{
    if (initialized)
        return;

    resetToDefaults();
    if (checkVersion())
    {
        // Now Serial should be ready
        EARLY_LOG("Version match - loading saved configs");
        loadAllConfigs();
    }
    else
    {
        EARLY_LOG("Version mismatch - using defaults");
        saveAllConfigs();
        updateVersion();
    }
    initialized = true;
}

ConfigManager::~ConfigManager()
{
    instance = nullptr;
}

ConfigManager *ConfigManager::getInstance()
{
    if (instance && !instance->initialized)
    {
        // Force initialization if someone tries to use before init() is called
        instance->init();
    }
    return instance;
}

// Static init method removed - use instance init() instead

// EEPROM operations
void ConfigManager::saveSteerConfig()
{
    // Pack boolean values into bytes for efficient storage
    uint8_t configByte1 = 0;
    uint8_t configByte2 = 0;

    if (invertWAS)
        configByte1 |= 0x01;
    if (motorDriveDirection)
        configByte1 |= 0x02;
    if (cytronDriver)
        configByte1 |= 0x04;
    if (steerSwitch)
        configByte1 |= 0x08;
    if (steerButton)
        configByte1 |= 0x10;
    if (shaftEncoder)
        configByte1 |= 0x20;

    if (pressureSensor)
        configByte2 |= 0x01;
    if (currentSensor)
        configByte2 |= 0x02;
    if (isUseYAxis)
        configByte2 |= 0x04;
    if (pwmBrakeMode)
        configByte2 |= 0x08;

    LOG_DEBUG(EventSource::CONFIG, "Saving steer config: button=%d, switch=%d, byte1=0x%02X",
              steerButton, steerSwitch, configByte1);

    int addr = STEER_CONFIG_ADDR;
    EEPROM.put(addr, configByte1);
    addr += sizeof(configByte1);
    EEPROM.put(addr, configByte2);
    addr += sizeof(configByte2);
    EEPROM.put(addr, softStartDurationMs);
    addr += sizeof(softStartDurationMs);
    EEPROM.put(addr, pulseCountMax);
    addr += sizeof(pulseCountMax);
    EEPROM.put(addr, minSpeed);
    addr += sizeof(minSpeed);
    EEPROM.put(addr, motorDriverConfig);

    // Verify the write
    uint8_t verifyByte1;
    EEPROM.get(STEER_CONFIG_ADDR, verifyByte1);
    LOG_DEBUG(EventSource::CONFIG, "Steer config verification: wrote=0x%02X, read=0x%02X",
              configByte1, verifyByte1);
}

void ConfigManager::loadSteerConfig()
{
    uint8_t configByte1, configByte2;

    int addr = STEER_CONFIG_ADDR;
    EEPROM.get(addr, configByte1);
    addr += sizeof(configByte1);
    EEPROM.get(addr, configByte2);
    addr += sizeof(configByte2);
    EEPROM.get(addr, softStartDurationMs);
    addr += sizeof(softStartDurationMs);
    EEPROM.get(addr, pulseCountMax);
    addr += sizeof(pulseCountMax);
    EEPROM.get(addr, minSpeed);
    addr += sizeof(minSpeed);
    EEPROM.get(addr, motorDriverConfig);

    // Unpack boolean values
    invertWAS = (configByte1 & 0x01) != 0;
    motorDriveDirection = (configByte1 & 0x02) != 0;
    cytronDriver = (configByte1 & 0x04) != 0;
    steerSwitch = (configByte1 & 0x08) != 0;
    steerButton = (configByte1 & 0x10) != 0;
    shaftEncoder = (configByte1 & 0x20) != 0;

    pressureSensor = (configByte2 & 0x01) != 0;
    currentSensor = (configByte2 & 0x02) != 0;
    isUseYAxis = (configByte2 & 0x04) != 0;
    pwmBrakeMode = (configByte2 & 0x08) != 0;
}

void ConfigManager::saveSteerSettings()
{
    LOG_DEBUG(EventSource::CONFIG, "Saving steer settings: Kp=%.1f, High=%d, Min=%d",
              kp, highPWM, minPWM);

    int addr = STEER_SETTINGS_ADDR;
    EEPROM.put(addr, kp);
    addr += sizeof(kp);
    EEPROM.put(addr, highPWM);
    addr += sizeof(highPWM);
    EEPROM.put(addr, minPWM);
    addr += sizeof(minPWM);
    EEPROM.put(addr, steerSensorCounts);
    addr += sizeof(steerSensorCounts);
    EEPROM.put(addr, wasOffset);
    addr += sizeof(wasOffset);
    EEPROM.put(addr, ackermanFix);

    // Verify the save
    uint8_t verifyHighPWM;
    EEPROM.get(STEER_SETTINGS_ADDR + sizeof(kp), verifyHighPWM);
    LOG_DEBUG(EventSource::CONFIG, "Steer settings verification: saved highPWM=%d, read back=%d",
              highPWM, verifyHighPWM);
}

void ConfigManager::loadSteerSettings()
{
    int addr = STEER_SETTINGS_ADDR;
    EEPROM.get(addr, kp);
    addr += sizeof(kp);
    EEPROM.get(addr, highPWM);
    addr += sizeof(highPWM);
    EEPROM.get(addr, minPWM);
    addr += sizeof(minPWM);
    EEPROM.get(addr, steerSensorCounts);
    addr += sizeof(steerSensorCounts);
    EEPROM.get(addr, wasOffset);
    addr += sizeof(wasOffset);
    EEPROM.get(addr, ackermanFix);

    LOG_DEBUG(EventSource::CONFIG, "Loaded steer settings: Kp=%.1f, High=%d, Min=%d",
              kp, highPWM, minPWM);
}

void ConfigManager::saveGPSConfig()
{
    int addr = GPS_CONFIG_ADDR;

    uint8_t gpsConfigByte = 0;
    if (gpsPassThrough)
        gpsConfigByte |= 0x01;

    EEPROM.put(addr, gpsConfigByte);
    addr += sizeof(gpsConfigByte);
    EEPROM.put(addr, serialRadioBaudRate);
}

void ConfigManager::loadGPSConfig()
{
    int addr = GPS_CONFIG_ADDR;

    uint8_t gpsConfigByte;
    EEPROM.get(addr, gpsConfigByte);
    addr += sizeof(gpsConfigByte);
    EEPROM.get(addr, serialRadioBaudRate);

    gpsPassThrough = (gpsConfigByte & 0x01) != 0;

    // Set default if not initialized
    if (serialRadioBaudRate == 0 || serialRadioBaudRate == 0xFFFFFFFF)
    {
        serialRadioBaudRate = 115200; // Default to 115200
    }
}

void ConfigManager::saveMachineConfig()
{
    int addr = MACHINE_CONFIG_ADDR;

    uint8_t machineConfigByte = 0;
    if (hydraulicLift)
        machineConfigByte |= 0x01;
    if (isPinActiveHigh)
        machineConfigByte |= 0x02;
    if (sectionControlSleepMode)
        machineConfigByte |= 0x04;

    EEPROM.put(addr, machineConfigByte);
    addr += sizeof(machineConfigByte);
    EEPROM.put(addr, raiseTime);
    addr += sizeof(raiseTime);
    EEPROM.put(addr, lowerTime);
}

void ConfigManager::loadMachineConfig()
{
    int addr = MACHINE_CONFIG_ADDR;

    uint8_t machineConfigByte;
    EEPROM.get(addr, machineConfigByte);
    addr += sizeof(machineConfigByte);
    EEPROM.get(addr, raiseTime);
    addr += sizeof(raiseTime);
    EEPROM.get(addr, lowerTime);

    hydraulicLift = (machineConfigByte & 0x01) != 0;
    isPinActiveHigh = (machineConfigByte & 0x02) != 0;
    sectionControlSleepMode = (machineConfigByte & 0x04) != 0;
}

void ConfigManager::saveINSConfig()
{
    int addr = INS_CONFIG_ADDR;

    uint8_t insConfigByte = 0;
    if (insUseFusion)
        insConfigByte |= 0x01;

    EEPROM.put(addr, insConfigByte);
}

void ConfigManager::loadINSConfig()
{
    int addr = INS_CONFIG_ADDR;

    uint8_t insConfigByte;
    EEPROM.get(addr, insConfigByte);

    insUseFusion = (insConfigByte & 0x01) != 0;
}

void ConfigManager::loadAllConfigs()
{
    loadNetworkConfig(); // Load network first as it might be needed by other modules
    loadSteerConfig();
    loadSteerSettings();
    loadGPSConfig();
    loadMachineConfig();
    loadINSConfig();
    loadTurnSensorConfig();
    loadAnalogWorkSwitchConfig();
    loadMiscConfig();
    loadCANSteerConfig(); // Load CAN configuration
    loadDNSAliasConfig();  // Load user DNS aliases
}

void ConfigManager::saveAllConfigs()
{
    saveNetworkConfig(); // Save network config too
    saveSteerConfig();
    saveSteerSettings();
    saveGPSConfig();
    saveMachineConfig();
    saveINSConfig();
    saveTurnSensorConfig();
    saveAnalogWorkSwitchConfig();
    saveMiscConfig();
    saveCANSteerConfig(); // Save CAN configuration
    saveDNSAliasConfig();  // Save user DNS aliases
}

void ConfigManager::resetToDefaults()
{
    // Steer config defaults
    invertWAS = false;
    motorDriveDirection = false;
    cytronDriver = false;
    steerSwitch = false;
    steerButton = false;
    shaftEncoder = false;
    pressureSensor = false;
    currentSensor = false;
    isUseYAxis = false;
    pwmBrakeMode = false;      // Default to coast mode
    softStartDurationMs = 500; // Default 500ms enabled (direction change = 250ms auto)
    pulseCountMax = 5;
    minSpeed = 3;
    motorDriverConfig = 0x00; // Default to DRV8701 with wheel encoder

    // Steer settings defaults
    kp = 40.0;
    highPWM = 255;
    minPWM = 10;
    steerSensorCounts = 30;
    wasOffset = 0;
    ackermanFix = 1.0;
    pwmFilterAlpha = 90;        // 90% old value, 10% new value
    pwmMinThresholdPct = 25;    // 25% of minPWM as minimum output threshold

    // GPS config defaults
    gpsPassThrough = false;
    serialRadioBaudRate = 115200; // Default serial radio baud rate

    // Machine config defaults
    hydraulicLift = false;
    raiseTime = 2;
    lowerTime = 4;
    isPinActiveHigh = false;
    sectionControlSleepMode = false; // Default: onboard section control active

    // INS config defaults
    insUseFusion = false;

    // LED defaults
    ledBrightness = 25; // 25% default brightness

    // Buzzer defaults (0=Quiet, 1=Loud, 2=Off)
    buzzerVolume = 1; // Default to loud mode for field use

    // JD PWM defaults
    jdPWMSensitivity = 5; // Middle sensitivity

    // Turn sensor defaults
    turnSensorType = 0;      // None
    encoderType = 1;         // Single channel
    turnMaxPulseCount = 5;   // Same as pulseCountMax default
    pressureThreshold = 100; // Middle of range
    currentThreshold = 100;  // Middle of range
    currentZeroOffset = 90;  // From NG-V6 code

    // JD PWM defaults
    jdPWMEnabled = false;
    jdPWMSensitivity = 5; // Middle sensitivity (1-10 scale)

    // Analog work switch defaults
    analogWorkSwitchEnabled = false;
    workSwitchSetpoint = 50;   // 50%
    workSwitchHysteresis = 20; // 20%
    invertWorkSwitch = false;

    // Network configuration defaults
    ipAddress[0] = 192;
    ipAddress[1] = 168;
    ipAddress[2] = 5;
    ipAddress[3] = 126;
    subnet[0] = 255;
    subnet[1] = 255;
    subnet[2] = 255;
    subnet[3] = 0;
    gateway[0] = 192;
    gateway[1] = 168;
    gateway[2] = 5;
    gateway[3] = 1;
    dns[0] = 8;
    dns[1] = 8;
    dns[2] = 8;
    dns[3] = 8;
    destIP[0] = 192;
    destIP[1] = 168;
    destIP[2] = 5;
    destIP[3] = 255; // Broadcast
    destPort = 9999;

    // CAN steering defaults
    canSteerConfig.brand = 0;        // Disabled
    canSteerConfig.can1Speed = 0;    // 250k
    canSteerConfig.can1Function = 0; // None
    canSteerConfig.can2Speed = 0;    // 250k
    canSteerConfig.can2Function = 0; // None
    canSteerConfig.can3Speed = 0;    // 250k
    canSteerConfig.can3Function = 0; // None
    canSteerConfig.moduleID = 0x1C;  // Default Keya module ID
    canSteerConfig.reserved[0] = 0;  // Keya CAN-WAS disabled by default

    eeVersion = CURRENT_EE_VERSION;

    // DNS alias defaults: gps and steer are pre-configured; slots 2 and 3 are empty.
    // These can be freely changed or cleared on the /dns-alias settings page.
    strncpy(dnsAlias[0], "gps",   11); dnsAlias[0][11] = '\0';
    strncpy(dnsAlias[1], "steer", 11); dnsAlias[1][11] = '\0';
    dnsAlias[2][0] = '\0';
    dnsAlias[3][0] = '\0';
}

bool ConfigManager::checkVersion()
{
    uint16_t storedVersion;
    EEPROM.get(EE_VERSION_ADDR, storedVersion);

    // If EEPROM is uninitialized (0 or 0xFFFF), initialize it
    if (storedVersion == 0 || storedVersion == 0xFFFF)
    {
        EARLY_LOG("EEPROM appears uninitialized, performing first-time setup");
        return false; // This will trigger saveAllConfigs() and updateVersion()
    }

    return (storedVersion == CURRENT_EE_VERSION);
}

void ConfigManager::updateVersion()
{
    LOG_DEBUG(EventSource::CONFIG, "Writing version %d to EEPROM address %d",
              CURRENT_EE_VERSION, EE_VERSION_ADDR);
    EEPROM.put(EE_VERSION_ADDR, (uint16_t)CURRENT_EE_VERSION);

    // Verify the write
    uint16_t verifyVersion;
    EEPROM.get(EE_VERSION_ADDR, verifyVersion);
    LOG_DEBUG(EventSource::CONFIG, "Version write verification: wrote=%d, read back=%d",
              CURRENT_EE_VERSION, verifyVersion);
}

void ConfigManager::saveTurnSensorConfig()
{
    LOG_DEBUG(EventSource::CONFIG, "Saving turn sensor config: Type=%d, EncoderType=%d",
              turnSensorType, encoderType);

    int addr = TURN_SENSOR_CONFIG_ADDR;
    EEPROM.put(addr, turnSensorType);
    addr += sizeof(turnSensorType);
    EEPROM.put(addr, encoderType);
    addr += sizeof(encoderType);
    EEPROM.put(addr, turnMaxPulseCount);
    addr += sizeof(turnMaxPulseCount);
    EEPROM.put(addr, pressureThreshold);
    addr += sizeof(pressureThreshold);
    EEPROM.put(addr, currentThreshold);
    addr += sizeof(currentThreshold);
    EEPROM.put(addr, currentZeroOffset);
    addr += sizeof(currentZeroOffset);
    EEPROM.put(addr, jdPWMEnabled);
    addr += sizeof(jdPWMEnabled);
    // Use the previously skipped byte for jdPWMSensitivity
    EEPROM.put(addr, jdPWMSensitivity);
    addr += sizeof(jdPWMSensitivity);
}

void ConfigManager::loadTurnSensorConfig()
{
    int addr = TURN_SENSOR_CONFIG_ADDR;
    EEPROM.get(addr, turnSensorType);
    addr += sizeof(turnSensorType);
    EEPROM.get(addr, encoderType);
    addr += sizeof(encoderType);
    EEPROM.get(addr, turnMaxPulseCount);
    addr += sizeof(turnMaxPulseCount);
    EEPROM.get(addr, pressureThreshold);
    addr += sizeof(pressureThreshold);
    EEPROM.get(addr, currentThreshold);
    addr += sizeof(currentThreshold);
    EEPROM.get(addr, currentZeroOffset);
    addr += sizeof(currentZeroOffset);
    EEPROM.get(addr, jdPWMEnabled);
    addr += sizeof(jdPWMEnabled);
    // Use the previously skipped byte for jdPWMSensitivity
    EEPROM.get(addr, jdPWMSensitivity);
    addr += sizeof(jdPWMSensitivity);

    LOG_DEBUG(EventSource::CONFIG, "Loaded turn sensor config: Type=%d, EncoderType=%d, JDPWM=%d",
              turnSensorType, encoderType, jdPWMEnabled);
}

void ConfigManager::saveAnalogWorkSwitchConfig()
{
    LOG_INFO(EventSource::CONFIG, "Saving analog work switch config to EEPROM: Enabled=%d, SP=%d%%, H=%d%%, Inv=%d",
             analogWorkSwitchEnabled, workSwitchSetpoint, workSwitchHysteresis, invertWorkSwitch);

    int addr = ANALOG_WORK_SWITCH_ADDR;
    EEPROM.put(addr, analogWorkSwitchEnabled);
    addr += sizeof(analogWorkSwitchEnabled);
    EEPROM.put(addr, workSwitchSetpoint);
    addr += sizeof(workSwitchSetpoint);
    EEPROM.put(addr, workSwitchHysteresis);
    addr += sizeof(workSwitchHysteresis);
    EEPROM.put(addr, invertWorkSwitch);
}

void ConfigManager::loadAnalogWorkSwitchConfig()
{
    int addr = ANALOG_WORK_SWITCH_ADDR;
    EEPROM.get(addr, analogWorkSwitchEnabled);
    addr += sizeof(analogWorkSwitchEnabled);
    EEPROM.get(addr, workSwitchSetpoint);
    addr += sizeof(workSwitchSetpoint);
    EEPROM.get(addr, workSwitchHysteresis);
    addr += sizeof(workSwitchHysteresis);
    EEPROM.get(addr, invertWorkSwitch);

    // Validate loaded values
    if (workSwitchSetpoint > 100)
    {
        workSwitchSetpoint = 50; // Default
    }
    if (workSwitchHysteresis < 5 || workSwitchHysteresis > 25)
    {
        workSwitchHysteresis = 20; // Default
    }

    LOG_INFO(EventSource::CONFIG, "Loaded analog work switch config from EEPROM: Enabled=%d, SP=%d%%, H=%d%%, Inv=%d",
             analogWorkSwitchEnabled, workSwitchSetpoint, workSwitchHysteresis, invertWorkSwitch);
}

void ConfigManager::saveMiscConfig()
{
    LOG_DEBUG(EventSource::CONFIG, "Saving misc config: LED=%d%%, BuzzerVol=%d, JD_PWM=%d",
              ledBrightness, buzzerVolume, jdPWMSensitivity);

    int addr = MISC_CONFIG_ADDR;
    EEPROM.put(addr, ledBrightness);
    addr += sizeof(ledBrightness);
    EEPROM.put(addr, buzzerVolume);
    addr += sizeof(buzzerVolume);
    EEPROM.put(addr, jdPWMSensitivity);
    addr += sizeof(jdPWMSensitivity);
    EEPROM.put(addr, pwmFilterAlpha);
    addr += sizeof(pwmFilterAlpha);
    EEPROM.put(addr, pwmMinThresholdPct);
}

void ConfigManager::loadMiscConfig()
{
    int addr = MISC_CONFIG_ADDR;
    EEPROM.get(addr, ledBrightness);
    addr += sizeof(ledBrightness);

    // Read buzzer volume (0=Quiet, 1=Loud, 2=Off)
    EEPROM.get(addr, buzzerVolume);
    addr += sizeof(buzzerVolume);

    EEPROM.get(addr, jdPWMSensitivity);
    addr += sizeof(jdPWMSensitivity);
    EEPROM.get(addr, pwmFilterAlpha);
    addr += sizeof(pwmFilterAlpha);
    EEPROM.get(addr, pwmMinThresholdPct);

    // Validate loaded values
    if (ledBrightness < 5 || ledBrightness > 100)
    {
        ledBrightness = 25; // Default
    }
    if (buzzerVolume > 2)
    {
        buzzerVolume = 1; // Default to loud for field use
    }
    if (jdPWMSensitivity < 1 || jdPWMSensitivity > 10)
    {
        jdPWMSensitivity = 5; // Default
    }
    if (pwmFilterAlpha > 97)
    {
        // Keep alpha below 1.0 so the filter remains responsive and cannot freeze output.
        pwmFilterAlpha = 90; // Default: 90% old value
    }
    if (pwmMinThresholdPct > 100)
    {
        pwmMinThresholdPct = 25; // Default: 25% of minPWM
    }

    LOG_INFO(EventSource::CONFIG, "Loaded misc config from EEPROM: LED=%d%%, BuzzerVol=%d, JD_PWM=%d",
             ledBrightness, buzzerVolume, jdPWMSensitivity);
}

void ConfigManager::saveNetworkConfig()
{
    int addr = NETWORK_CONFIG_ADDR;

    // Write a marker byte to indicate valid config
    uint8_t marker = 0xAA;
    EEPROM.put(addr, marker);
    addr += sizeof(marker);

    // Save network configuration
    for (int i = 0; i < 4; i++)
    {
        EEPROM.put(addr, ipAddress[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.put(addr, subnet[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.put(addr, gateway[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.put(addr, dns[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.put(addr, destIP[i]);
        addr += sizeof(uint8_t);
    }
    EEPROM.put(addr, destPort);

    LOG_INFO(EventSource::CONFIG, "Saved network config - IP: %d.%d.%d.%d",
             ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
}

void ConfigManager::loadNetworkConfig()
{
    int addr = NETWORK_CONFIG_ADDR;

    // Check for valid config marker
    uint8_t marker;
    EEPROM.get(addr, marker);
    addr += sizeof(marker);

    if (marker != 0xAA)
    {
        LOG_INFO(EventSource::CONFIG, "No valid network config found, using defaults");
        return;
    }

    // Load network configuration
    for (int i = 0; i < 4; i++)
    {
        EEPROM.get(addr, ipAddress[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.get(addr, subnet[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.get(addr, gateway[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.get(addr, dns[i]);
        addr += sizeof(uint8_t);
    }
    for (int i = 0; i < 4; i++)
    {
        EEPROM.get(addr, destIP[i]);
        addr += sizeof(uint8_t);
    }
    EEPROM.get(addr, destPort);

    LOG_INFO(EventSource::CONFIG, "Loaded network config - IP: %d.%d.%d.%d",
             ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
}

// CAN Steer configuration methods
CANSteerConfig ConfigManager::getCANSteerConfig() const
{
    return canSteerConfig;
}

void ConfigManager::setCANSteerConfig(const CANSteerConfig &config)
{
    canSteerConfig = config;
}

void ConfigManager::saveCANSteerConfig()
{
    // We'll store this at the end of the EEPROM space
    // Using address 900 (plenty of room after other configs)
    int addr = 900;

    // Write a marker byte to indicate valid config
    uint8_t marker = 0xCA; // 'CA' for CAN
    EEPROM.put(addr, marker);
    addr += sizeof(marker);

    // Save the entire struct
    EEPROM.put(addr, canSteerConfig);

    LOG_INFO(EventSource::CONFIG, "Saved CAN Steer config - Brand: %d",
             canSteerConfig.brand);
}

void ConfigManager::loadCANSteerConfig()
{
    int addr = 900;

    // Check for valid config marker
    uint8_t marker;
    EEPROM.get(addr, marker);
    addr += sizeof(marker);

    if (marker != 0xCA)
    {
        LOG_INFO(EventSource::CONFIG, "No valid CAN Steer config found, using defaults");
        canSteerConfig = CANSteerConfig(); // Use default values
        return;
    }

    // Load the entire struct
    EEPROM.get(addr, canSteerConfig);

    // Reserved flags: currently only bit 0 is used (Keya CAN-WAS option).
    canSteerConfig.reserved[0] &= 0x01;

    LOG_INFO(EventSource::CONFIG, "Loaded CAN Steer config - Brand: %d",
             canSteerConfig.brand);
}

void ConfigManager::saveDNSAliasConfig()
{
    int addr = DNS_ALIAS_CONFIG_ADDR;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 12; j++) {
            EEPROM.put(addr++, dnsAlias[i][j]);
        }
    }
    LOG_DEBUG(EventSource::CONFIG, "Saved DNS aliases: '%s','%s','%s','%s'",
              dnsAlias[0], dnsAlias[1], dnsAlias[2], dnsAlias[3]);
}

void ConfigManager::loadDNSAliasConfig()
{
    int addr = DNS_ALIAS_CONFIG_ADDR;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 12; j++) {
            EEPROM.get(addr++, dnsAlias[i][j]);
        }
        dnsAlias[i][11] = '\0'; // Ensure null termination
        // Validate: only alphanumeric and hyphen allowed; 0xFF = uninitialized EEPROM
        for (int j = 0; j < 11; j++) {
            char c = dnsAlias[i][j];
            if (c == '\0') break;
            if (!isalnum((unsigned char)c) && c != '-') {
                dnsAlias[i][0] = '\0'; // Clear invalid entry
                break;
            }
        }
    }
    LOG_DEBUG(EventSource::CONFIG, "Loaded DNS aliases: '%s','%s','%s','%s'",
              dnsAlias[0], dnsAlias[1], dnsAlias[2], dnsAlias[3]);
}