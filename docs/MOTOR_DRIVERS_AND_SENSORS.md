# Motor Drivers and Sensors

This document describes the motor driver implementations and sensor interfaces in AiO v26.

## Motor Driver System

### Overview

AiO v26 supports multiple motor driver types through a unified interface. The system automatically detects and configures the appropriate driver based on hardware detection or EEPROM settings.

### Motor Driver Interface

All motor drivers implement the `MotorDriverInterface`:

```cpp
class MotorDriverInterface {
public:
    virtual void setPWM(int16_t pwm) = 0;
    virtual void enable() = 0;
    virtual void disable() = 0;
    virtual bool isEnabled() const = 0;
    virtual bool isHealthy() const = 0;
    virtual const char* getType() const = 0;
};
```

### Supported Motor Drivers

#### PWM Motor Drivers

**Cytron MD30C**
- Type: PWM + Direction
- PWM Frequency: 20kHz
- Max Current: 30A continuous
- Voltage: 5-30V
- Features: Overcurrent protection

**IBT-2 (BTS7960)**
- Type: Dual H-Bridge
- PWM Frequency: 20kHz  
- Max Current: 43A
- Voltage: 6-27V
- Features: Enable pin, thermal protection

**DRV8701**
- Type: H-Bridge gate driver
- PWM Frequency: 20kHz
- External MOSFETs required
- Features: Sleep mode, fault detection

#### CAN Motor Driver

**Keya Steering Motor**
- Communication: CAN bus (250kbps)
- Protocol: Custom Keya protocol
- Features: Position feedback, status monitoring
- Auto-detection at startup

#### Hydraulic Valve Driver

**Danfoss PVEA Valve**
- Type: Proportional valve
- Control: PCA9685 outputs 7 & 8
- PWM Frequency: 250Hz
- Center Position: 50% PWM
- Protected outputs: 5 & 6 reserved

### Motor Driver Manager

The `MotorDriverManager` handles driver selection and initialization:

```cpp
MotorDriverManager::getInstance()
    ├── Auto-detection (Keya CAN)
    ├── EEPROM configuration
    ├── Factory pattern creation
    └── Health monitoring
```

### PWM Control Details

#### PWM Signal Generation
- Hardware PWM on pin 4
- 16-bit resolution
- Configurable frequency (typ. 20kHz)
- Dead-band compensation

#### Direction Control
- Digital output on pin 5
- HIGH = Forward/Right
- LOW = Reverse/Left
- Synchronized with PWM

#### Enable/Sleep Control
- Enable pin 6 (IBT-2)
- Sleep pin 7 (DRV8701)
- Active HIGH logic
- Safety disable on fault

### CAN Motor Protocol

#### Keya Message Format

**Command Message (0x6000001)**:
```cpp
struct KeyaCommand {
    uint16_t speed;      // Motor speed (0-1000)
    uint8_t direction;   // 0=stop, 1=CW, 2=CCW
    uint8_t enable;      // 0=disable, 1=enable
};
```

**Status Message (0x7000001)**:
```cpp
struct KeyaStatus {
    uint16_t position;   // Current position
    uint8_t status;      // Status flags
    uint8_t error;       // Error code
};
```

### Motor Control Features

#### Coast/Brake Mode
- Configurable per driver type
- Coast: Outputs tri-state when disabled
- Brake: Outputs LOW when disabled
- Safety consideration for steering

#### Current Limiting
- Hardware current sensing
- Software PWM limiting
- Thermal protection
- Overcurrent shutdown

#### Fault Detection
- Driver fault pins monitored
- CAN timeout detection
- Current sensor validation
- Automatic recovery attempts

## Sensor Systems

### Wheel Angle Sensor (WAS)

Primary steering feedback sensor:

**Hardware Interface**:
- Analog input on pin A0
- 0-5V input range
- 12-bit ADC resolution
- 1kHz sampling rate

**Calibration**:
- Center point calibration
- Counts per degree scaling
- Ackerman correction
- Stored in EEPROM

**Signal Processing**:
- Moving average filter
- Deadband elimination
- Range validation
- Noise rejection

### Encoder Support

Secondary angle measurement:

**Single Channel Mode**:
- Pin 2 input only
- Direction from WAS
- Pulse counting

**Quadrature Mode**:
- Pin 2 (A) and Pin 3 (B)
- Direction detection
- 4x resolution
- Interrupt driven

**Features**:
- High-speed counting
- Rollover handling
- Position tracking
- Fusion with WAS

### Current Sensor

Motor current monitoring:

**Hardware**:
- Analog input on pin A1
- ACS712 or similar
- ±30A typical range
- 66mV/A sensitivity

**Calibration**:
- Zero offset calibration
- Stored in EEPROM
- Temperature compensation
- Automatic at startup

**Usage**:
- Kickout detection
- Motor health monitoring
- Power consumption
- Stall detection

### Pressure Sensor

Hydraulic pressure monitoring:

**Interface**:
- Analog input on pin A3
- 0-5V input range
- Configurable scaling
- Threshold detection

**Applications**:
- Hydraulic fault detection
- Load monitoring
- Safety limits
- System diagnostics

### Switch Inputs

Digital control inputs:

**Work Switch**:
- Pin A2 (analog mode)
- Threshold detection
- Debouncing
- Active low/high configurable

**Remote Switch**:
- Pin 36 (digital)
- Internal pullup
- Interrupt capable
- Safety override

**Steer Switch**:
- Pin 32 (digital)
- Manual control button
- Blue LED feedback
- Toggle operation

### Sensor Fusion

**WheelAngleFusion** combines multiple sensors:

```cpp
WheelAngleFusion
    ├── Primary: WAS analog
    ├── Secondary: Encoder
    ├── Fusion algorithm
    └── Output: Filtered angle
```

**Benefits**:
- Improved accuracy
- Redundancy
- Noise reduction
- Fault tolerance

## Autosteer Control Loop

### Control Architecture

```
Setpoint → PID Controller → Motor Driver → Steering System
    ↑                                            ↓
    └──────── WAS/Encoder Feedback ←────────────┘
```

### Update Rates
- Control loop: 100Hz (10ms)
- WAS sampling: 1kHz
- PGN status: 100Hz
- Current monitoring: 100Hz

### PID Tuning

**Proportional Gain**:
- Typical range: 100-200
- Higher for hydraulic
- Lower for motor drive

**Integral Gain**:
- Usually disabled (0)
- Small values if needed
- Prevents windup

**Derivative Gain**:
- Typical range: 0-50
- Damping oscillations
- Noise sensitive

### Safety Features

**Kickout Monitor**:
- Current threshold
- Encoder error detection
- Time-based limits
- Automatic disengage

**Watchdog Timer**:
- Communication timeout
- Control loop monitoring
- Automatic safe state
- Error reporting

## Configuration and Calibration

### Motor Configuration

Via web interface or serial menu:
1. Select motor type
2. Configure PWM settings
3. Set current limits
4. Test motor direction

### WAS Calibration

Step-by-step process:
1. Center wheels straight
2. Press calibrate button
3. Turn full left, record
4. Turn full right, record
5. Calculate counts/degree

### Encoder Setup

Configuration options:
- Single vs quadrature
- Pulses per revolution
- Gear ratio
- Direction reversal

### Current Sensor Zero

Automatic calibration:
1. Ensure motor off
2. Sample current reading
3. Calculate offset
4. Store in EEPROM

## Troubleshooting

### Motor Issues

**No Movement**:
- Check enable signal
- Verify PWM output
- Test direction pin
- Monitor current

**Erratic Movement**:
- Check PWM frequency
- Verify connections
- Test without load
- Check supply voltage

**CAN Motor Timeout**:
- Verify CAN termination
- Check baud rate
- Monitor CAN traffic
- Test heartbeat

### Sensor Issues

**WAS Noise**:
- Check grounding
- Add filtering
- Verify supply voltage
- Shield cables

**Encoder Missing Counts**:
- Check connections
- Verify voltage levels
- Test pull-up resistors
- Monitor interrupts

**Current Sensor Drift**:
- Recalibrate zero
- Check temperature
- Verify supply stability
- Replace if needed

## Best Practices

### Motor Control
1. Start with low PWM values
2. Implement proper ramping
3. Monitor current always
4. Use appropriate dead-band

### Sensor Integration
1. Validate all readings
2. Implement timeout handling
3. Use appropriate filtering
4. Calibrate regularly

### Safety Implementation
1. Multiple disengage methods
2. Fail-safe defaults
3. Clear error indication
4. Automatic recovery where safe