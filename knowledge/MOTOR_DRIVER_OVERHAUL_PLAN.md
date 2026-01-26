# Motor Driver Overhaul Implementation Plan

## Overview
This plan outlines the phased implementation to support three motor driver types with automatic detection and configuration based on the V6 implementation.

## Current State Analysis

### V6 Implementation
- **Keya CAN**: Detected by heartbeat (0x7000001), controlled via CAN commands
- **Danfoss**: Configured via PGN251 Byte 8, uses Output 5 (enable) and Output 6 (analog PWM)
- **DRV8701P**: Default driver, uses PWM1/PWM2 pins with complementary control

### v26 Current State
- Already has `MotorDriverInterface` abstraction
- Has `KeyaCANDriver` implementation (partially complete)
- Has `PWMMotorDriver` for standard PWM control
- Has `MotorDriverFactory` for driver instantiation
- Missing: Danfoss driver, proper detection logic, kickout handling

## Detection Priority Order
1. **Keya CAN** - Heartbeat detection on CAN3
2. **Danfoss** - EEPROM configuration (set via PGN251)
3. **DRV8701P** - Default fallback

## Configuration Mapping (PGN251 Byte 8)
Configuration is sent when user updates settings in AOG and stored in EEPROM:
- 0x00: DRV8701P + Wheel Encoder
- 0x01: Danfoss + Wheel Encoder
- 0x02: DRV8701P + Pressure Sensor
- 0x03: Danfoss + Pressure Sensor
- 0x04: DRV8701P + Current Sensor

## Implementation Phases

### Phase 1: Foundation & Detection Logic (2-3 days)
**Objective**: Establish detection framework and update interfaces

**Tasks**:
1. Update `MotorDriverInterface` to include:
   ```cpp
   virtual bool isDetected() = 0;
   virtual MotorDriverType getType() = 0;
   virtual void handleKickout(KickoutType type, float value) = 0;
   virtual float getCurrentDraw() = 0;
   ```

2. Create `MotorDriverDetector` class:
   - Integrate with existing Keya heartbeat detection at startup
   - Read driver configuration from EEPROM (originally set by PGN251 Byte 8)
   - Implement detection state machine
   - Add detection logging

3. Update `SteerConfig` structure to include:
   - `IsDanfoss` flag
   - `KickoutType` enum (Encoder, Pressure, Current)
   - Motor driver configuration stored in EEPROM

**Test Points**:
- Test with live Keya motor connected
- Send PGN251 with various Byte 8 values and verify EEPROM storage
- Verify detection priority order

**Deliverables**:
- Updated interfaces
- Detection logic implementation
- Compile, test, debug and commit

---

### Phase 2: Keya CAN Driver Verification (1 day)
**Objective**: Verify and complete existing Keya implementation

**Tasks**:
1. Verify existing `KeyaCANDriver` functionality:
   - Confirm heartbeat monitoring is working
   - Verify speed mapping (-255 to 255 → -995 to 995)
   - Confirm enable/disable commands work
   - Test encoder/speed query functionality

2. Update kickout handling:
   - Use existing "motor slip" method from v26
   - Remove current sensor query (not required)
   - Remove version query (not required)

**Test Points**:
- Test with actual Keya motor
- Verify speed control accuracy
- Test motor slip detection
- Verify kickout functionality

**Deliverables**:
- Verified Keya driver functionality
- Any necessary bug fixes
- Compile, test, debug and commit

---

### Phase 3: Danfoss Valve Driver (2-3 days)
**Objective**: Implement Danfoss valve control

**Tasks**:
1. Create `DanfossMotorDriver` class:
   ```cpp
   class DanfossMotorDriver : public MotorDriverInterface {
       // Output 5: Enable pin (HIGH = enabled)
       // Output 6: Analog control (PWM value mapped to 25%-75% range)
   };
   ```

2. Implement control logic:
   - Take standard PWM value (0-255) from autosteer calculation
   - Map to Danfoss range: 0 → 64 (25%), 128 → 128 (50%), 255 → 192 (75%)
   - Control Output 5 for enable/disable
   - Implement 12V analog output on Output 6
   - Use existing center position from steering wizard

3. Configure outputs:
   - Use machine output pins 5 & 6
   - Use PWM frequencies from autosteer.h
   - Add safety limits

**Test Points**:
- Measure Output 6 voltage (should be 3-9V range)
- Test enable/disable on Output 5
- Verify smooth control response
- Test with actual Danfoss valve

**Deliverables**:
- Danfoss driver implementation
- Voltage measurement logs
- Compile, test, debug and commit

---

### Phase 4: DRV8701P Driver Update (1-2 days)
**Objective**: Update PWM driver to match V6 behavior

**Tasks**:
1. Update `PWMMotorDriver` to `DRV8701PMotorDriver`:
   - Ensure complementary PWM operation
   - Inactive pin must be LOW (not HIGH)
   - Add current sensor support via CURRENT_PIN
   - Implement sleep mode control

2. PWM control logic:
   ```cpp
   if (speed > 0) {
       analogWrite(PWM2_PIN, 255);      // Turn off first
       analogWrite(PWM1_PIN, 255 - pwm); // Then activate
   } else {
       analogWrite(PWM1_PIN, 255);      // Turn off first
       analogWrite(PWM2_PIN, 255 - pwm); // Then activate
   }
   ```

3. Add current monitoring:
   - Read from CURRENT_PIN (A13)
   - Implement filtering
   - Support kickout threshold

**Test Points**:
- Verify PWM signals on scope
- Test current sensor readings
- Verify motor direction control
- Test kickout functionality

**Deliverables**:
- Updated DRV8701P driver
- Current sensor calibration data
- Compile, test, debug and commit

---

### Phase 5: Kickout System Integration (2 days)
**Objective**: Unified kickout handling across all drivers

**Tasks**:
1. Create `KickoutMonitor` class:
   - Support multiple input types
   - Configurable thresholds
   - Debouncing logic
   - Event generation

2. Input handling:
   - Wheel encoder (KICKOUT_D_PIN) - single input only
   - Pressure sensor (KICKOUT_A_PIN)
   - Current sensor (driver-specific)

3. Integration:
   - Each driver registers with KickoutMonitor
   - Monitor calls driver's handleKickout()
   - Add kickout event logging

**Test Points**:
- Test each kickout type
- Verify threshold settings
- Test debouncing
- Verify all drivers respond correctly

**Deliverables**:
- Kickout monitoring system
- Integration with all drivers
- Compile, test, debug and commit

---

### Phase 6: Factory & Auto-Selection (1 day)
**Objective**: Automatic driver selection and instantiation at startup

**Tasks**:
1. Update `MotorDriverFactory`:
   - Integrate with MotorDriverDetector
   - Implement detection → instantiation flow
   - Add fallback logic
   - Driver selection occurs only at startup

2. AutosteerProcessor integration:
   - Remove hardcoded driver selection
   - Use factory for driver creation at initialization
   - Update status reporting

**Test Points**:
- Test automatic detection at startup
- Verify fallback to DRV8701P
- Test PGN251 configuration changes (requires reboot)

**Deliverables**:
- Updated factory implementation
- Auto-selection logic
- Compile, test, debug and commit

---

### Phase 7: Testing & Optimization (2-3 days)
**Objective**: Full system validation

**Tasks**:
1. Integration testing:
   - Test each driver type individually
   - Verify kickout configurations
   - Test configuration changes and reboot
   - Performance profiling

2. Fault recovery testing:
   - CAN bus disconnection (should disable autosteer)
   - Invalid PGN251 values (should use EEPROM or default)
   - Kickout during active steering
   - Power cycling behavior

3. Documentation:
   - Update user manual
   - Create troubleshooting guide
   - Document configuration options

**Test Points**:
- Field testing with each driver type
- Stress testing
- Configuration validation
- Performance benchmarks

**Deliverables**:
- Test results
- Bug fixes
- Documentation

---

## Risk Mitigation

1. **Hardware Availability**: Ensure access to all three motor types for testing
2. **Backward Compatibility**: Maintain existing functionality during transition
3. **Safety**: Implement failsafe modes for each driver type
4. **Testing Coverage**: Use available hardware (live Keya motor) for testing

## Success Criteria

1. Detection works reliably at startup for all three motor types
2. Each driver provides equivalent steering performance
3. Kickout functionality works correctly for all configurations
4. No regression in existing functionality
5. Clean, maintainable code structure

## Timeline Estimate

Total: 11-14 days of development

- Phase 1: 2-3 days
- Phase 2: 1 day  
- Phase 3: 2-3 days
- Phase 4: 1-2 days
- Phase 5: 2 days
- Phase 6: 1 day
- Phase 7: 2-3 days

## Next Steps

1. Review and approve this plan
2. Begin Phase 1 implementation
3. Schedule regular testing checkpoints with available hardware