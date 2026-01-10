# Hardware Ownership Matrix for AiO v26

## Overview
This document defines clear hardware ownership for the AiO v26 firmware to prevent conflicts and ensure proper initialization.

## Pin Ownership Matrix

### Analog Input Pins
| Pin | Name | Current Config | Proper Owner | Conflicts |
|-----|------|----------------|--------------|-----------|
| A12 | KICKOUT_A_PIN | HardwareManager: INPUT<br>ADProcessor: INPUT_DISABLE | **ADProcessor** (pressure)<br>**EncoderProcessor** (encoder) | Yes - Multiple configs |
| A13 | CURRENT_PIN | HardwareManager: INPUT<br>ADProcessor: INPUT_DISABLE | **ADProcessor** | Yes - Wrong mode in HW |
| A15 | WAS_SENSOR_PIN | HardwareManager: INPUT_DISABLE<br>ADProcessor: INPUT_DISABLE | **ADProcessor** | No |
| A17 | WORK_PIN | HardwareManager: commented out<br>ADProcessor: INPUT_PULLUP | **ADProcessor** | No |

### Digital Input Pins
| Pin | Name | Current Config | Proper Owner | Conflicts |
|-----|------|----------------|--------------|-----------|
| 2 | STEER_PIN | HardwareManager: commented out<br>ADProcessor: INPUT_PULLUP | **ADProcessor** | No |
| 3 | KICKOUT_D_PIN | HardwareManager: INPUT_PULLUP<br>KickoutMonitor: INPUT_PULLUP | **EncoderProcessor** | Potential |

### Output Pins
| Pin | Name | Current Config | Proper Owner | Conflicts |
|-----|------|----------------|--------------|-----------|
| 4 | SLEEP_PIN | HardwareManager: OUTPUT | **MotorDriverInterface** | No |
| 5 | PWM1_PIN | HardwareManager: OUTPUT | **PWMMotorDriver** | No |
| 6 | PWM2_PIN | HardwareManager: OUTPUT | **PWMMotorDriver** | No |
| 33 | SPEEDPULSE_PIN | Not initialized | **PWMProcessor** | Missing init |
| 36 | BUZZER | HardwareManager: OUTPUT | **HardwareManager** | No |

## Identified Conflicts

### 1. KICKOUT_A_PIN (A12)
- **Issue**: Configured as INPUT by HardwareManager, INPUT_DISABLE by ADProcessor
- **Impact**: Pull-up resistor enabled, causing 100% pressure readings
- **Solution**: Remove from HardwareManager, let ADProcessor/EncoderProcessor manage

### 2. CURRENT_PIN (A13)
- **Issue**: Configured as INPUT by HardwareManager instead of INPUT_DISABLE
- **Impact**: Potential analog reading issues
- **Solution**: Remove from HardwareManager, let ADProcessor manage

### 3. Dynamic Pin Reconfiguration
- **Issue**: KICKOUT_A switches between analog (pressure) and digital (encoder)
- **Current**: Both modules try to manage the pin
- **Solution**: Clear ownership transfer protocol

## Shared Resources

### 1. ADC (Analog-to-Digital Converter)
- **Users**: ADProcessor (WAS, current, pressure)
- **Settings**: Resolution, averaging, speed
- **Current**: Each module sets its own ADC config
- **Need**: Coordinated ADC configuration

### 2. PWM Frequencies
- **Motor PWM**: 18.3kHz (PWM1/PWM2)
- **Speed Pulse**: Variable frequency
- **Current**: Set by HardwareManager
- **Need**: Keep centralized

### 3. I2C Buses
- **Wire**: PCA9685 for sections (0x44)
- **Wire2**: PCA9685 for LEDs (0x70)
- **Management**: I2CManager handles initialization

### 4. Serial Ports
- **Management**: SerialManager owns all UART configuration
- **No conflicts**: Clear ownership

## Recommendations

### Phase 1 Actions
1. Document current state (COMPLETE)
2. Identify all conflicts (COMPLETE)
3. Define proper ownership (COMPLETE)

### Phase 2 Actions
1. Remove pin initialization from HardwareManager for:
   - KICKOUT_A_PIN
   - CURRENT_PIN
   - Already commented: STEER_PIN, WORK_PIN

2. Keep in HardwareManager:
   - Pin definitions (getters)
   - PWM frequency management
   - Buzzer control

### Phase 3 Actions
1. Ensure each module initializes its owned pins
2. Add ownership comments to each module
3. Implement clean handoff for dynamic pins

### Timing Dependencies
1. HardwareManager must init before other modules (for pin definitions)
2. ADProcessor should init early (provides sensor readings)
3. Motor drivers init after ADProcessor (may need current sensing)
4. EncoderProcessor can init anytime (self-contained)

## Benefits After Implementation
- No more pin configuration conflicts
- Clear ownership makes debugging easier
- Dynamic reconfiguration (encoder↔pressure) works reliably
- Easier to add new hardware features
- Portable to different PCB versions