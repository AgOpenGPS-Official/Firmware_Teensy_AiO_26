// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef CONFIGMANAGER_H_
#define CONFIGMANAGER_H_

#include "Arduino.h"
#include <EEPROM.h>
#include "EEPROMLayout.h"

// CAN bus functions (bitfield - multiple can be selected)
enum class CANFunction : uint8_t {
    NONE      = 0x00,  // No function
    STEERING  = 0x01,  // Steering control (valve/motor)
    BUTTONS   = 0x02,  // Button inputs (engage/disengage)
    HITCH     = 0x04,  // 3-point hitch control
    IMPLEMENT = 0x08,  // ISO implement control
    KEYA      = 0x10   // Keya motor (special case)
};

enum class CANBusName : uint8_t {
    NONE = 0,
    KEYA = 1,
    V_BUS = 2,
    K_BUS = 3,
    ISO_BUS = 4
};

// CAN Steer configuration structure
struct CANSteerConfig {
    // Brand selection
    uint8_t brand = 9;          // 0=Disabled, 1=Fendt, 2=Valtra, etc, 9=Generic (default)

    // CAN1 configuration
    uint8_t can1Speed = 0;      // 0=250k, 1=500k
    uint8_t can1Function = 0;   // CANFunction enum
    uint8_t can1Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    // CAN2 configuration
    uint8_t can2Speed = 0;      // 0=250k, 1=500k
    uint8_t can2Function = 0;   // CANFunction enum
    uint8_t can2Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    // CAN3 configuration
    uint8_t can3Speed = 0;      // 0=250k, 1=500k
    uint8_t can3Function = 0;   // CANFunction enum
    uint8_t can3Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    uint8_t moduleID = 0x1C;    // Module ID for protocols that need it
    uint8_t reserved[1];        // Bit 0: allow CAN curve as WAS source for Keya mode
};

class ConfigManager
{
private:
    static ConfigManager *instance;

    // Steer configuration (EEPROM 200-299)
    bool invertWAS;
    bool motorDriveDirection;
    bool cytronDriver;
    bool steerSwitch;
    bool steerButton;
    bool shaftEncoder;
    bool pressureSensor;
    bool currentSensor;
    bool isUseYAxis;
    bool pwmBrakeMode;  // false = coast mode (default), true = brake mode
    uint16_t softStartDurationMs;  // 0=disabled, 100-1000ms enabled (soft-accel is auto-calculated as 1/2)
    uint8_t pulseCountMax;
    uint8_t minSpeed;
    uint8_t motorDriverConfig;  // From PGN251 Byte 8

    // Steer settings (EEPROM 300-399)
    float kp;
    uint8_t highPWM;
    uint8_t minPWM;
    uint8_t steerSensorCounts;
    int16_t wasOffset;
    float ackermanFix;
    uint8_t pwmFilterAlpha;        // Low-pass filter coefficient (0-97 = 0%-97% old value), default 90
    uint8_t pwmMinThresholdPct;    // Minimum output threshold as % of minPWM (0-100), default 25

    // GPS configuration (EEPROM 400-499)
    bool gpsPassThrough;
    uint32_t serialRadioBaudRate;  // RTK radio baud rate (4800-921600)

    // Machine settings (EEPROM 500-599)
    bool hydraulicLift;
    uint8_t raiseTime;
    uint8_t lowerTime;
    bool isPinActiveHigh;
    // Internal storage flag: true means onboard section control is inactive (sleep).
    bool sectionControlSleepMode;

    // INS configuration (EEPROM 700-799)
    bool insUseFusion;

    // LED settings
    uint8_t ledBrightness;

    // Buzzer settings (0=Quiet, 1=Loud, 2=Off)
    uint8_t buzzerVolume;

    // Turn sensor configuration
    uint8_t turnSensorType;      // 0=None, 1=Encoder, 2=Pressure, 3=Current, 4=JD PWM
    uint8_t encoderType;         // 1=Single, 2=Quadrature
    uint8_t turnMaxPulseCount;   // Max encoder pulses before kickout
    uint8_t pressureThreshold;   // Pressure sensor threshold
    uint8_t currentThreshold;    // Current sensor threshold
    uint16_t currentZeroOffset;  // Current sensor zero offset

    // John Deere PWM encoder configuration
    bool jdPWMEnabled;           // Enable JD PWM mode for pressure input
    uint8_t jdPWMSensitivity;    // JD PWM sensitivity 1-10 (1=least sensitive, 10=most sensitive)

    // Analog work switch configuration
    bool analogWorkSwitchEnabled;
    uint8_t workSwitchSetpoint;     // 0-100% stored as 0-100
    uint8_t workSwitchHysteresis;   // 5-25% stored as 5-25
    bool invertWorkSwitch;

    // Network configuration
    uint8_t ipAddress[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    uint8_t dns[4];
    uint8_t destIP[4];
    uint16_t destPort;

    // Version control
    uint16_t eeVersion;

    // CAN Steer configuration
    CANSteerConfig canSteerConfig;

    // DNS alias configuration (EEPROM DNS_ALIAS_CONFIG_ADDR)
    // Up to 4 user-configurable shortnames (without ".aog") that resolve to the Teensy IP.
    // Example: "board" → board.aog → 192.168.5.126
    char dnsAlias[4][12];  // max 11 chars + null terminator each

    // Initialization tracking
    bool initialized;

public:
    ConfigManager();
    ~ConfigManager();

    // Singleton access
    static ConfigManager *getInstance();
    void init();

    // Steer configuration methods
    bool getInvertWAS() const { return invertWAS; }
    void setInvertWAS(bool value) { invertWAS = value; }
    bool getMotorDriveDirection() const { return motorDriveDirection; }
    void setMotorDriveDirection(bool value) { motorDriveDirection = value; }
    bool getCytronDriver() const { return cytronDriver; }
    void setCytronDriver(bool value) { cytronDriver = value; }
    bool getSteerSwitch() const { return steerSwitch; }
    void setSteerSwitch(bool value) { steerSwitch = value; }
    bool getSteerButton() const { return steerButton; }
    void setSteerButton(bool value) { steerButton = value; }
    bool getShaftEncoder() const { return shaftEncoder; }
    void setShaftEncoder(bool value) { shaftEncoder = value; }
    bool getPressureSensor() const { return pressureSensor; }
    void setPressureSensor(bool value) { pressureSensor = value; }
    bool getCurrentSensor() const { return currentSensor; }
    void setCurrentSensor(bool value) { currentSensor = value; }
    bool getIsUseYAxis() const { return isUseYAxis; }
    void setIsUseYAxis(bool value) { isUseYAxis = value; }
    bool getPWMBrakeMode() const { return pwmBrakeMode; }
    void setPWMBrakeMode(bool value) { pwmBrakeMode = value; }
    uint16_t getSoftStartDurationMs() const { return softStartDurationMs; }
    void setSoftStartDurationMs(uint16_t value) { softStartDurationMs = constrain(value, 0, 1000); }
    uint8_t getPulseCountMax() const { return pulseCountMax; }
    void setPulseCountMax(uint8_t value) { pulseCountMax = value; }
    uint8_t getMinSpeed() const { return minSpeed; }
    void setMinSpeed(uint8_t value) { minSpeed = value; }
    uint8_t getMotorDriverConfig() const { return motorDriverConfig; }
    void setMotorDriverConfig(uint8_t value) { motorDriverConfig = value; }

    // Steer settings methods
    float getKp() const { return kp; }
    void setKp(float value) { kp = value; }
    uint8_t getHighPWM() const { return highPWM; }
    void setHighPWM(uint8_t value) { highPWM = value; }
    uint8_t getMinPWM() const { return minPWM; }
    void setMinPWM(uint8_t value) { minPWM = value; }
    uint8_t getSteerSensorCounts() const { return steerSensorCounts; }
    void setSteerSensorCounts(uint8_t value) { steerSensorCounts = value; }
    int16_t getWasOffset() const { return wasOffset; }
    void setWasOffset(int16_t value) { wasOffset = value; }
    float getAckermanFix() const { return ackermanFix; }
    void setAckermanFix(float value) { ackermanFix = value; }
    uint8_t getPwmFilterAlpha() const { return pwmFilterAlpha; }
    void setPwmFilterAlpha(uint8_t value) { pwmFilterAlpha = constrain(value, 0, 97); }
    uint8_t getPwmMinThresholdPct() const { return pwmMinThresholdPct; }
    void setPwmMinThresholdPct(uint8_t value) { pwmMinThresholdPct = value; }

    // LED configuration
    uint8_t getLEDBrightness() const { return ledBrightness; }
    void setLEDBrightness(uint8_t value) {
        ledBrightness = constrain(value, 5, 100);
    }

    // Buzzer configuration (0=Quiet, 1=Loud, 2=Off)
    uint8_t getBuzzerVolume() const { return buzzerVolume; }
    void setBuzzerVolume(uint8_t value) { buzzerVolume = (value <= 2) ? value : 1; }

    // GPS configuration methods
    bool getGPSPassThrough() const { return gpsPassThrough; }
    void setGPSPassThrough(bool value) { gpsPassThrough = value; }
    uint32_t getSerialRadioBaudRate() const { return serialRadioBaudRate; }
    void setSerialRadioBaudRate(uint32_t value) { serialRadioBaudRate = value; }

    // Machine configuration methods
    bool getHydraulicLift() const { return hydraulicLift; }
    void setHydraulicLift(bool value) { hydraulicLift = value; }
    uint8_t getRaiseTime() const { return raiseTime; }
    void setRaiseTime(uint8_t value) { raiseTime = value; }
    uint8_t getLowerTime() const { return lowerTime; }
    void setLowerTime(uint8_t value) { lowerTime = value; }
    bool getIsPinActiveHigh() const { return isPinActiveHigh; }
    void setIsPinActiveHigh(bool value) { isPinActiveHigh = value; }
    bool getSectionControlSleepMode() const { return sectionControlSleepMode; }
    void setSectionControlSleepMode(bool value) { sectionControlSleepMode = value; }

    // INS configuration methods
    bool getINSUseFusion() const { return insUseFusion; }
    void setINSUseFusion(bool value) { insUseFusion = value; }

    // Turn sensor configuration methods
    uint8_t getTurnSensorType() const { return turnSensorType; }
    void setTurnSensorType(uint8_t value) { turnSensorType = value; }
    uint8_t getEncoderType() const { return encoderType; }
    void setEncoderType(uint8_t value) { encoderType = value; }
    uint8_t getTurnMaxPulseCount() const { return turnMaxPulseCount; }
    void setTurnMaxPulseCount(uint8_t value) { turnMaxPulseCount = value; }
    uint8_t getPressureThreshold() const { return pressureThreshold; }
    void setPressureThreshold(uint8_t value) { pressureThreshold = value; }
    uint8_t getCurrentThreshold() const { return currentThreshold; }
    void setCurrentThreshold(uint8_t value) { currentThreshold = value; }
    uint16_t getCurrentZeroOffset() const { return currentZeroOffset; }
    void setCurrentZeroOffset(uint16_t value) { currentZeroOffset = value; }

    // John Deere PWM encoder methods
    bool getJDPWMEnabled() const { return jdPWMEnabled; }
    void setJDPWMEnabled(bool value) { jdPWMEnabled = value; }
    uint8_t getJDPWMSensitivity() const { return jdPWMSensitivity; }
    void setJDPWMSensitivity(uint8_t value) { jdPWMSensitivity = constrain(value, 1, 10); }

    // Analog work switch methods
    bool getAnalogWorkSwitchEnabled() const { return analogWorkSwitchEnabled; }
    void setAnalogWorkSwitchEnabled(bool value) { analogWorkSwitchEnabled = value; }
    uint8_t getWorkSwitchSetpoint() const { return workSwitchSetpoint; }
    void setWorkSwitchSetpoint(uint8_t value) { workSwitchSetpoint = constrain(value, 0, 100); }
    uint8_t getWorkSwitchHysteresis() const { return workSwitchHysteresis; }
    void setWorkSwitchHysteresis(uint8_t value) { workSwitchHysteresis = constrain(value, 5, 25); }
    bool getInvertWorkSwitch() const { return invertWorkSwitch; }
    void setInvertWorkSwitch(bool value) { invertWorkSwitch = value; }

    // Network configuration methods
    void getIPAddress(uint8_t* ip) const { memcpy(ip, ipAddress, 4); }
    void setIPAddress(const uint8_t* ip) { memcpy(ipAddress, ip, 4); }
    void getSubnet(uint8_t* sub) const { memcpy(sub, subnet, 4); }
    void setSubnet(const uint8_t* sub) { memcpy(subnet, sub, 4); }
    void getGateway(uint8_t* gw) const { memcpy(gw, gateway, 4); }
    void setGateway(const uint8_t* gw) { memcpy(gateway, gw, 4); }
    void getDNS(uint8_t* d) const { memcpy(d, dns, 4); }
    void setDNS(const uint8_t* d) { memcpy(dns, d, 4); }
    void getDestIP(uint8_t* dest) const { memcpy(dest, destIP, 4); }
    void setDestIP(const uint8_t* dest) { memcpy(destIP, dest, 4); }
    uint16_t getDestPort() const { return destPort; }
    void setDestPort(uint16_t port) { destPort = port; }

    // EEPROM operations
    void saveSteerConfig();
    void loadSteerConfig();
    void saveSteerSettings();
    void loadSteerSettings();
    void saveGPSConfig();
    void loadGPSConfig();
    void saveMachineConfig();
    void loadMachineConfig();
    void saveINSConfig();
    void loadINSConfig();
    void saveTurnSensorConfig();
    void loadTurnSensorConfig();
    void saveAnalogWorkSwitchConfig();
    void loadAnalogWorkSwitchConfig();
    void saveMiscConfig();
    void loadMiscConfig();
    void saveNetworkConfig();
    void loadNetworkConfig();
    void loadAllConfigs();
    void saveAllConfigs();
    void resetToDefaults();
    bool checkVersion();
    void updateVersion();

    // CAN Steer configuration methods
    CANSteerConfig getCANSteerConfig() const;
    void setCANSteerConfig(const CANSteerConfig& config);
    void saveCANSteerConfig();
    void loadCANSteerConfig();

    // DNS alias configuration methods
    const char* getDNSAlias(uint8_t idx) const { return (idx < 4) ? dnsAlias[idx] : ""; }
    void setDNSAlias(uint8_t idx, const char* value) {
        if (idx < 4) { strncpy(dnsAlias[idx], value, 11); dnsAlias[idx][11] = 0; }
    }
    void saveDNSAliasConfig();
    void loadDNSAliasConfig();
};

#endif // CONFIGMANAGER_H_
