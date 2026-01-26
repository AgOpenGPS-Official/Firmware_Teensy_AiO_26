# AiO v26 Front Panel LED Status Guide

This guide explains the meaning of the four status LEDs on the front panel of the AiO v26 board.

## LED Overview

The front panel has four status LEDs arranged horizontally:

1. **PWR/ETH** - Power and Ethernet Status
2. **GPS** - GPS Status
3. **STEER** - Autosteer Status  
4. **INS** - IMU/INS Status

Each LED can display different colors and patterns to indicate various states.

## Special Visual Indicators

- **Blue Pulse on GPS LED**: Brief blue flash indicates RTCM correction data received
- **Blue Pulse on STEER LED**: Brief blue flash indicates button press

---

## PWR/ETH LED (Power/Ethernet)

This LED indicates power, Ethernet connection, and AgIO communication status.

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🔴 **RED** | Solid | System booting or no Ethernet connection |
| 🟡 **YELLOW** | Solid | Ethernet connected but no AgIO communication |
| 🟢 **GREEN** | Solid | Ethernet connected and AgIO communication active |

**Notes:**
- During boot (first 5 seconds), the LED shows RED
- AgIO is considered "connected" when the board is receiving data from AgIO
- The connection timeout is 10 seconds - if no data is received for 10 seconds, status reverts to yellow

---

## GPS LED

This LED indicates GPS receiver status and fix quality.

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🔴 **RED** | Solid | No GPS data being received |
| 🟡 **YELLOW** | Solid | GPS data received, any fix quality except RTK Fixed |
| 🟢 **GREEN** | Solid | RTK Fixed (full RTK solution, best accuracy) |
| 🔵 **BLUE** | Brief pulse | RTCM correction data received (overlays current color) |

**Fix Quality Details:**
- **No Fix**: GPS receiver is powered but cannot determine position
- **GPS/DGPS**: Standard positioning, accuracy typically 1-3 meters  
- **RTK Float**: Partial RTK solution, accuracy typically 20-50 cm
- **RTK Fixed**: Full RTK solution, accuracy typically 2-5 cm

**Note:** The brief blue pulse indicates incoming RTCM corrections and helps verify the correction data stream is active.

---

## INS LED (Inertial Navigation System)

This LED indicates the status of the IMU or INS (Inertial Navigation System).

| Color | Pattern | Meaning |
|-------|---------|---------|
| ⚫ **OFF** | - | No IMU data on serial port |
| 🔴 **RED** | Solid | Data received but not valid IMU format |
| 🟡 **YELLOW** | Solid | IMU detected but not yet providing valid data or aligning |
| 🟢 **GREEN** | Solid | IMU fully operational with valid data |

**Supported IMU Types:**
- **BNO08x**: External IMU module
- **TM171**: Alternative external IMU
- **UM981 INS**: Integrated INS in UM981 GPS module

**INS Alignment States (UM981):**
- During alignment (status < 3), the LED shows yellow
- Once fully aligned (status = 3), the LED turns green

**Note:** The LED reflects actual data validity, not just detection

---

## STEER LED (Autosteer)

This LED indicates autosteer system status and hardware health.

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🔴 **RED** | Solid | WAS (Wheel Angle Sensor) or other hardware malfunction |
| 🟡 **YELLOW** | Solid | Steering ready but not engaged |
| 🟢 **GREEN** | Solid | Steering engaged and active |
| 🔵 **BLUE** | Brief pulse | Button press detected (overlays current color) |

**State Details:**
- **Malfunction (Red)**: WAS reading out of range, motor driver error, or other hardware issue
- **Ready (Yellow)**: All systems operational, waiting for engagement
- **Engaged (Green)**: Autosteer is actively controlling the steering

**Note:** The LED state is managed by the AutosteerProcessor and reflects the actual steering system status

---

## Startup Sequence

During initialization, the LEDs will briefly flash green:
1. All LEDs turn GREEN for 100ms
2. Normal operation begins immediately

The PWR/ETH LED will show RED during the boot phase (first 5 seconds) before transitioning to its operational state based on network connectivity.

---

## Troubleshooting

### PWR/ETH LED Issues
- **Stays Red**: Check Ethernet cable and connection
- **Stays Yellow**: Verify AgIO is running and configured for the correct network
- **Flashing between states**: Check for intermittent network issues

### GPS LED Issues
- **Stays Off**: Check GPS module connection and power
- **Stays Red**: Verify antenna connection and sky visibility
- **Never reaches Green (RTK)**: Check NTRIP/base station configuration

### INS LED Issues
- **Stays Off**: Check serial connection to IMU module
- **Stays Red**: Invalid data format - verify IMU type and baud rate
- **Stays Yellow**: Allow more time for alignment/calibration, ensure vehicle is moving for INS alignment

### STEER LED Issues
- **Shows Red**: Check WAS voltage (should be 0.5-4.5V), verify motor driver health
- **Stays Yellow**: Normal when autosteer is not engaged
- **Won't turn Green**: Verify autosteer engagement in AgOpenGPS
- **Blue flashes**: Button presses are being detected (normal)

---

## LED Brightness

The LED brightness is set to 25% by default for optimal visibility without being too bright. The brightness can be adjusted programmatically if needed. The LEDs use a PCA9685 PWM controller at address 0x70 for precise color and brightness control.

## Technical Details

- **LED Controller**: PCA9685 at I2C address 0x70
- **PWM Frequency**: 120Hz to avoid flicker
- **Update Rate**: LEDs are updated every 100ms
- **FSM Implementation**: LEDs are managed by a finite state machine (LEDManagerFSM) for reliable state transitions