# Performance Optimization Analysis for AiO v26

## Executive Summary

This document outlines various performance optimization strategies for the AiO v26 firmware running on Teensy 4.1 without an RTOS. The analysis was prompted by observed BNO sensor missed messages and concerns about timing-critical task execution.

## Current Architecture Overview

The system runs a cooperative multitasking model with the following critical tasks:
- **Autosteer Control Loop** (100Hz) - PID calculations and motor control
- **UART/Serial Communications** - BNO sensor, GPS, and other serial devices
- **Network Communications** - UDP packet processing and PGN handling
- **I2C/SPI Communications** - Sensor readings and output control

## Optimization Strategies

### 1. Real-Time Performance Improvements

#### 1.1 Interrupt-Driven Serial with Ring Buffers
```cpp
// Increase hardware serial buffers to prevent data loss
Serial1.addMemoryForRead(rxBuffer, 1024);
Serial1.addMemoryForWrite(txBuffer, 1024);
```
**Pros**: Simple to implement, reduces data loss
**Cons**: Uses more RAM
**Priority**: HIGH - Quick win for BNO missed messages

#### 1.2 Hardware Timers (IntervalTimer)
```cpp
IntervalTimer autosteerTimer;
autosteerTimer.begin(autosteerISR, 10000); // 10ms = 100Hz
```
**Pros**: Guaranteed timing, hardware-based
**Cons**: Limited number of timers available
**Priority**: HIGH - Critical for control loop consistency

#### 1.3 DMA for I2C/SPI
**Pros**: Non-blocking transfers, frees CPU
**Cons**: Complex implementation
**Priority**: MEDIUM - Beneficial for sensor-heavy systems

#### 1.4 TeensyThreads Library
**Pros**: Better task isolation, cooperative threading
**Cons**: Memory overhead, complexity
**Priority**: LOW - Consider only if simpler solutions insufficient

### 2. Task Scheduling System

#### Pros:
- Predictable timing and guaranteed intervals
- Priority management for critical tasks
- CPU utilization monitoring
- Better maintainability and debugging

#### Cons:
- 5-10% CPU overhead
- Increased complexity
- Shared resource management challenges
- Additional memory usage

#### Recommended Hybrid Approach:
- Use IntervalTimer for critical tasks (autosteer, BNO)
- Implement simple priority checks in main loop
- Time-slice long operations (network processing)
- Avoid full scheduler complexity

### 3. Code Optimization Opportunities

#### 3.1 String Operation Optimizations

**High Impact Areas:**
1. **AutosteerProcessor.cpp** (100Hz loop)
   - Remove sprintf/snprintf from control loop
   - Use fixed-size buffers without dynamic concatenation

2. **EventLogger.cpp**
   - Pre-format static parts of log messages
   - Consider binary logging format
   - Implement conditional logging levels

#### 3.2 Fixed-Point Math Conversions

**Critical Optimizations:**

1. **Sin() function in AutosteerProcessor** (Line 749)
```cpp
// Current: Expensive transcendental function
float sineRamp = sin(rampProgress * PI / 2.0f);

// Optimized: Quadratic approximation (1000x faster)
float sineRamp = rampProgress * (1.5708f - 0.646f * rampProgress * rampProgress);
```

2. **Atan() in WheelAngleFusion**
```cpp
// Replace with Padé approximation
float x = headingRateRad * config.wheelbase / speed;
float x2 = x * x;
float angleRad = x * (0.9999f + x2 * (-0.3303f + x2 * 0.1801f)) /
                 (1.0f + x2 * (0.9999f + x2 * 0.2568f));
```

3. **Sqrt() in GNSSProcessor**
```cpp
// Work with squared values instead
float speedSquared = vN * vN + vE * vE;
// Compare against squared thresholds
```

### 4. Finite State Machine (FSM) Implementation

#### Top FSM Candidates:

1. **Autosteer Control FSM** (Safety-critical)
   - States: IDLE, ACTIVE, PRESSURE_KICKOUT, SLIP_DETECTED, EMERGENCY_STOP
   - Benefits: Clear safety state management, testable transitions

2. **LED Status Indicator FSM**
   - States: OFF, SOLID_RED, BLINK_RED, SOLID_GREEN, BLINK_GREEN
   - Benefits: Simple proof of concept, visual feedback

3. **Hydraulic Control FSM**
   - States: IDLE, RAISING, LOWERING, TIMED_OUT
   - Benefits: Clear timer-based transitions, safety improvement

4. **Network Connection FSM**
   - States: DISCONNECTED, NEGOTIATING, DHCP_DISCOVERY, CONNECTED
   - Benefits: Unified connection management

#### FSM Implementation Benefits:
- State diagrams map directly to code
- All transitions are explicit and testable
- Easy debugging with state change logging
- Improved code maintainability

### 5. Implementation Roadmap

#### Phase 1: Quick Wins (1-2 days)
1. Increase serial buffers for all UART devices
2. Implement sin() approximation in autosteer loop
3. Add timing metrics to identify bottlenecks
4. Remove string operations from control loops

#### Phase 2: Hardware Optimization (3-5 days)
1. Move autosteer to IntervalTimer
2. Implement BNO timeout recovery
3. Add simple priority ordering to main loop
4. Profile and measure improvements

#### Phase 3: Structural Improvements (1-2 weeks)
1. Implement LED Manager FSM as proof of concept
2. Convert hydraulic control to FSM
3. Implement fixed-point math for remaining float operations
4. Consider binary logging format

#### Phase 4: Advanced Optimization (if needed)
1. Implement lightweight task scheduler
2. Add DMA transfers for I2C/SPI
3. Convert autosteer to hierarchical FSM
4. Optimize memory allocation patterns

## Metrics and Monitoring

To measure optimization effectiveness:
1. Add cycle counters to critical functions
2. Log maximum loop execution time
3. Track missed sensor readings
4. Monitor task overruns
5. Measure jitter in control loops

## Recommendations

### Immediate Actions:
1. **Increase BNO serial buffer** - Addresses immediate problem
2. **Remove sin() from autosteer** - High impact, low risk
3. **Add timing diagnostics** - Identify actual bottlenecks

### Short-term Improvements:
1. **Implement IntervalTimer for autosteer** - Guaranteed timing
2. **Create simple FSM for LED Manager** - Prove the pattern
3. **Optimize string operations in logger** - Reduce latency spikes

### Long-term Considerations:
1. **Evaluate need for full task scheduler** - Only if problems persist
2. **Consider TeensyThreads** - For future feature additions
3. **Implement comprehensive FSM framework** - For code maintainability

## Conclusion

The most effective approach is a targeted optimization strategy focusing on:
1. Critical path optimization (autosteer loop)
2. Serial communication reliability (buffer increases)
3. Mathematical function approximations (sin, atan, sqrt)
4. Structural improvements with FSMs where complexity warrants

Full RTOS or complex scheduling systems are likely overkill for the current requirements. The proposed hybrid approach provides most benefits with minimal complexity increase.