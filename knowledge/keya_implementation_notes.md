# Keya Motor Implementation - Eliminating the Kludge

## The Problem with V6-NG Implementation

The old implementation had a kludge where PWM values were converted to CAN commands:
```cpp
// In autosteerPID.h:
SteerKeya(pwmDrive);  // Mixing PWM concept with CAN protocol!
```

## The v26 Solution

### 1. **Proper Abstraction**
The autosteer code now uses a clean interface:
```cpp
motor->setSpeed(50.0f);  // 50% forward - works for ANY motor type
```

### 2. **Keya 20ms Command Requirement**
The Keya motor requires commands to be sent continuously every 20ms. This is now handled automatically in the `process()` method:

```cpp
// In the main loop:
motor->process();  // Call this regularly

// Inside KeyaCANMotorDriver::process():
if (status.enabled && (millis() - lastCommandMs >= COMMAND_INTERVAL_MS)) {
    // Automatically resend the last speed command every 20ms
    sendSpeedCommand(lastKeyaSpeed);
}
```

### 3. **Key Features**

1. **Automatic Command Repeat**: The driver automatically resends commands every 20ms
2. **Safety on Communication Loss**: If CAN sends fail repeatedly, the motor is disabled
3. **Heartbeat Monitoring**: Tracks Keya motor heartbeat messages
4. **Clean Separation**: Autosteer logic doesn't know about CAN protocols

### 4. **Protocol Details (from V6-NG)**

Speed Command (ID: 0x0CFE1A27):
- Byte 0: 0xF8 (fixed)
- Byte 1: 0xFF (fixed)
- Byte 2: Direction (0x00 = CCW, 0x01 = CW)
- Byte 3-4: Speed value (0-995) big-endian
- Byte 5-7: 0xFF (fixed)

Enable Command (ID: 0x0CF00400):
- All bytes: {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}

Disable Command (ID: 0x0CF00301):
- All bytes: {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}

### 5. **Usage in Autosteer**

```cpp
// Initialize
MotorDriverInterface* motor = MotorDriverFactory::createMotorDriver(
    MotorDriverType::KEYA_CAN, hwMgr, canMgr
);
motor->init();

// In the control loop (must be called frequently!)
void loop() {
    // Calculate steer command
    float steerCommand = pidController->compute(error);
    
    // Set motor speed - clean interface!
    motor->setSpeed(steerCommand);
    
    // CRITICAL: Process motor updates (handles 20ms resend)
    motor->process();  // This MUST be called regularly!
    
    // The motor will automatically:
    // - Resend commands every 20ms
    // - Monitor heartbeats
    // - Handle safety shutdowns
}
```

### 6. **Benefits Over the Kludge**

1. **No PWM-to-CAN conversion in application code**
2. **Automatic handling of Keya's 20ms requirement**
3. **Proper error handling and safety features**
4. **Same interface works for all motor types**
5. **Easy to test and maintain**

The key insight: The autosteer shouldn't care about motor protocols. It just commands speeds through a clean interface, and each driver handles its own protocol requirements internally.