# CANProtocolEngine: Data-Driven CAN Protocol System

## Executive Summary

The JSON configs in `CanConfig_Examples/` already define everything needed for brand-specific CAN behavior, but the firmware ignores them and hard-codes ~1,200 lines of brand-specific logic across 4 files. This plan proposes a generalized `CANProtocolEngine` that reads the JSON configs at runtime, eliminating per-brand C++ code.

**Impact:**
- Remove ~1,220 lines of hard-coded brand logic
- Add ~560 lines of generic, config-driven code
- Adding new tractor brands requires **only a JSON config file** - no firmware changes

---

## Current State

### JSON Configs Already Define (But Firmware Ignores)

Every brand config in `CanConfig_Examples/` includes protocol details that map directly to the hard-coded firmware logic:

```json
"canConfig": {
  "VFilter": "0x0CEF2CF0",           // CAN IDs to receive (valve status)
  "VReceiveCurve": "0x0CEF2CF0",     // Valve feedback CAN ID
  "ReceiveCurveLoHi": "0,1",         // Byte positions for curve value
  "ValveState": 2,                   // Byte index for valve ready check
  "curveMod": -32128,                // Curve offset for calculation
  "VSendCurve": "0x0CEFF02C",        // CAN ID for sending steer commands
  "VDataLen": 6,                     // Message length
  "SendCurve": "5,9,3,10,0,0",       // Byte template for steer message
  "SendCurveLoHi": "4,5",            // Byte positions for curve in command
  "ITS1": "2,253",                   // Intent-to-steer byte (enabled)
  "ITS0": "2,252"                    // Intent-to-steer byte (disabled)
},
"steer": [
  {
    "buttonLabel": "Engage button",
    "bus": "K_Bus",
    "canFilterID": "0x18EF1C32",
    "byte": 2,
    "onStateAND": 1
  }
]
```

### Hard-Coded Firmware (~1,200 lines)

| Category | File | Lines | What's Hard-coded |
|----------|------|-------|-------------------|
| Engagement Detection | TractorCANDriver.cpp | ~200 | 7 process functions, 7 boolean flags |
| Valve Ready Detection | TractorCANDriver.cpp | ~200 | 8 process functions, CAN IDs, byte checks |
| Steering Commands | TractorCANDriver.cpp | ~400 | 8 send functions, message structures |
| CAN Filters | CANFilterHelper.cpp | ~280 | 9 brand-specific filter setup functions |
| Message Routing | TractorCANDriver.cpp | ~100 | 2 switch-case dispatchers |
| State Tracking | TractorCANDriver.h | ~50 | Per-brand flags and accessors |
| Edge Detection | AutosteerProcessor.cpp | ~120 | Per-brand static variables and checks |

---

## JSON Config Field Inventory

### Universal Fields (All Brands)

| Field | Type | Purpose |
|-------|------|---------|
| `VFilter` | hex string | CAN IDs to receive (comma-separated) |
| `VSendCurve` | hex string | CAN ID for outgoing steer commands |
| `SendCurve` | comma-separated bytes | Byte template for steer message |
| `SendCurveLoHi` | "lo,hi" | Byte positions for curve value in command |
| `ITS1` | "pos,val" | Intent-to-steer enabled (byte position, value) |
| `ITS0` | "pos,val" | Intent-to-steer disabled (byte position, value) |

### Optional Fields

| Field | Type | Used By | Purpose |
|-------|------|---------|---------|
| `VReceiveCurve` | hex string | Valtra, Claas | CAN ID for receiving curve feedback |
| `ReceiveCurveLoHi` | "lo,hi" | Valtra, Claas | Byte positions for received curve |
| `ValveState` | number | Claas | Byte index for valve ready check |
| `curveMod` | number | Fendt | Curve offset value (-32128) |
| `VDataLen` | number | Fendt | Outgoing message length |
| `SendCurveSteer` | byte string | Fendt | Full byte template when steering active |
| `SendCurveNoSteer` | byte string | Fendt | Full byte template when steering inactive |
| `CurveSteerLoHi` | "lo,hi" | Fendt | Curve byte positions (Fendt-specific) |
| `VBusBaud` | number | Fendt One | CAN bus baud rate override |

### Model-Level Fields

| Field | Type | Purpose |
|-------|------|---------|
| `model` | string | Human-readable model name |
| `vClaim` | hex string | V-Bus arbitration priority (part of CAN address) |
| `steer[]` | array | Engage button definitions |
| `work[]` | array | Work/implement control definitions |
| `canConfig` | object | Model-specific overrides of brand canConfig |

### Steer Array Entry

```json
{
  "buttonLabel": "Fendt armrest button",
  "bus": "K_Bus",
  "canFilterID": "0x613",
  "byte": 1,
  "onStateAND": 4
}
```

### vClaim Values

The `vClaim` field identifies the module's V-Bus address for CAN message construction:

| Brand | vClaim | Used in CAN IDs |
|-------|--------|-----------------|
| Fendt | `2C` | 0x0CEFF0**2C**, 0x0CEF**2C**F0 |
| Case IH / NH | `AA` | 0x0CAD08**AA**, 0x0CAC**AA**08 |
| Valtra | `1C` | 0x0CAD13**1C**, 0x0CAC**1C**13 |
| Claas | `1E` | 0x0CAD13**1E**, 0x0CAC**1E**13 |
| JCB | `AB` | 0x0CAD13**AB**, 0x0CAC**AB**13 |
| Lindner | `F0` | 0x0CAD**F0**13, 0x0CAC**F0**13 |

---

## Proposed Architecture

### CANProtocolEngine

A single class that replaces all brand-specific message handling:

```cpp
class CANProtocolEngine {
public:
    // Configuration
    bool loadConfig(const char* json, size_t len);
    bool isConfigured() const;

    // Incoming message processing (replaces all process*Message functions)
    void processIncomingMessage(uint8_t busNum, const CAN_message_t& msg);

    // Outgoing command sending (replaces all send*Commands functions)
    void sendSteerCommand(int16_t curve, bool steerActive, void* canBus);

    // Engagement detection
    bool checkEngageEvent();
    const char* getLastEngageLabel() const;
    void resetEngageFlags();

    // Valve ready status
    bool isValveReady() const;
    bool isValveDataReceived() const;
    int16_t getActualCurve() const;

    // CAN filter setup
    void setupFilters(uint8_t busNum, void* canBus);

private:
    // Parsed engagement rules
    CANEngageRule engageRules[MAX_ENGAGE_RULES];
    uint8_t engageRuleCount = 0;

    // Parsed receive config
    CANReceiveConfig receiveConfig;

    // Parsed send config
    CANSendConfig sendConfig;

    // Parsed CAN filter IDs
    uint32_t filterIds[MAX_FILTER_IDS];
    uint8_t filterIdCount = 0;

    // Runtime state
    bool valveReady = false;
    bool valveDataReceived = false;
    int16_t actualCurve = 0;
    uint32_t lastValveReadyTime = 0;
    char lastEngageLabel[32] = "";
};
```

### Config Structures

```cpp
// Valve feedback / steering position reception
struct CANReceiveConfig {
    uint32_t canId;             // VReceiveCurve or VFilter[0]
    uint8_t curveLoBytePos;     // ReceiveCurveLoHi[0]
    uint8_t curveHiBytePos;     // ReceiveCurveLoHi[1]
    uint8_t valveStateBytePos;  // ValveState (byte to check for ready)
    int16_t curveMod;           // curveMod (offset applied to curve)
    bool littleEndian;          // Byte order for curve value
    bool configured;            // Whether receive config was found in JSON
};

// Steering command sending
struct CANSendConfig {
    uint32_t canId;             // VSendCurve
    uint8_t dataLen;            // VDataLen (default 8)
    uint8_t templateSteer[8];   // SendCurve or SendCurveSteer
    uint8_t templateNoSteer[8]; // SendCurveNoSteer (or derived from ITS0)
    uint8_t curveLoBytePos;     // SendCurveLoHi[0]
    uint8_t curveHiBytePos;     // SendCurveLoHi[1]
    uint8_t intentBytePos;      // ITS1[0] / ITS0[0]
    uint8_t intentEnabled;      // ITS1[1]
    uint8_t intentDisabled;     // ITS0[1]
    int16_t curveMod;           // curveMod (offset for sending)
    bool littleEndian;          // Byte order for curve value
    bool configured;
};

// Single byte match condition
struct CANByteCondition {
    uint8_t byteIndex;
    uint8_t mask;
    uint8_t expectedValue;
};

// Engagement button rule (supports multi-byte conditions)
struct CANEngageRule {
    uint32_t canId;
    CANByteCondition conditions[4];     // Up to 4 conditions (AND logic)
    uint8_t conditionCount;
    bool useFallingEdge;                // true for Fendt/Massey (button release)
    bool isExtendedId;                  // 29-bit vs 11-bit CAN ID
    char label[32];                     // For logging

    // Runtime state (not serialized)
    bool currentState = false;
    bool previousState = false;
    bool eventTriggered = false;
};

static constexpr uint8_t MAX_ENGAGE_RULES = 8;
static constexpr uint8_t MAX_FILTER_IDS = 8;
```

---

## JSON Config Parsing

### CANConfigParser

```cpp
class CANConfigParser {
public:
    // Parse brand-level canConfig
    static bool parseReceiveConfig(const JsonObject& canConfig, CANReceiveConfig& out);
    static bool parseSendConfig(const JsonObject& canConfig, CANSendConfig& out);
    static bool parseFilterIds(const JsonObject& canConfig, uint32_t* ids, uint8_t& count);

    // Parse model-level steer array
    static bool parseEngageRules(const JsonArray& steerArray,
                                 CANEngageRule* rules, uint8_t& count);

    // Helpers
    static uint32_t parseHexString(const char* hex);
    static void parseCommaSeparatedBytes(const char* str, uint8_t* out, uint8_t& count);
    static void parseBytePositions(const char* str, uint8_t& lo, uint8_t& hi);
};
```

### Parsing Flow

```
JSON from LittleFS (CANConfigStorage)
    │
    ├─ brands[selectedBrand].canConfig
    │   ├─ VFilter            → filterIds[]
    │   ├─ VReceiveCurve      → receiveConfig.canId
    │   ├─ ReceiveCurveLoHi   → receiveConfig.curveLoBytePos/Hi
    │   ├─ ValveState         → receiveConfig.valveStateBytePos
    │   ├─ curveMod           → receiveConfig.curveMod / sendConfig.curveMod
    │   ├─ VSendCurve         → sendConfig.canId
    │   ├─ SendCurve          → sendConfig.templateSteer[]
    │   ├─ SendCurveLoHi      → sendConfig.curveLoBytePos/Hi
    │   ├─ ITS1               → sendConfig.intentBytePos + intentEnabled
    │   └─ ITS0               → sendConfig.intentBytePos + intentDisabled
    │
    └─ brands[selectedBrand].models[selectedModel].steer[]
        ├─ canFilterID    → engageRules[].canId
        ├─ byte           → engageRules[].conditions[0].byteIndex
        ├─ onStateAND     → engageRules[].conditions[0].mask + expectedValue
        ├─ buttonLabel    → engageRules[].label
        ├─ edgeType       → engageRules[].useFallingEdge (optional, default rising)
        └─ conditions[]   → engageRules[].conditions[] (multi-byte, optional)
```

### Backward-Compatible Engage Rules

Single-byte format (current):
```json
{ "canFilterID": "0x613", "byte": 1, "onStateAND": 4 }
```

Multi-byte format (new, for Case IH etc.):
```json
{
  "canFilterID": "0x14FF7706",
  "conditions": [
    { "byte": 0, "mask": 255, "value": 130 },
    { "byte": 1, "mask": 255, "value": 1 }
  ],
  "logic": "AND"
}
```

Parser handles both:
```cpp
if (steerObj.containsKey("conditions")) {
    // Parse multi-byte conditions array
} else {
    // Parse legacy single-byte format
    rule.conditions[0] = { steerObj["byte"], steerObj["onStateAND"], steerObj["onStateAND"] };
    rule.conditionCount = 1;
}
```

---

## Edge Cases and Brand Variations

### Fendt: Unique Steering Template

Fendt uses separate full-byte templates for steer/no-steer rather than just an intent byte:

```json
"SendCurveSteer": "5,9,3,10,0,0",
"SendCurveNoSteer": "5,9,2,10,0,0"
```

The engine handles this by storing both templates. If `SendCurveSteer` and `SendCurveNoSteer` are present, use them directly. Otherwise, build from `SendCurve` + `ITS1`/`ITS0`.

### CAT MT: Value-Range Valve Ready

Most brands: valve ready = byte[N] != 0
CAT MT: valve ready = curve value between 15000-17000

JSON extension:
```json
"valveReadyCondition": "range:15000:17000"
```

Default if absent: `"nonzero"` (current behavior for all other brands).

### Fendt: Standard CAN ID (11-bit)

Fendt K_Bus uses `0x613` (standard 11-bit), while all other brands use extended 29-bit CAN IDs. The parser detects this automatically:

```cpp
rule.isExtendedId = (rule.canId > 0x7FF);
```

### Case IH: Multiple Engage Button Variants

Case IH has 6 different `steer` entries for different model years - all loaded as separate rules:

```json
"steer": [
  { "buttonLabel": "Puma CVX 160 2015", "canFilterID": "0x18FFB306", "byte": 2, "onStateAND": 1 },
  { "buttonLabel": "Puma CVX 165 2022", "canFilterID": "0x14FF7706", "byte": 0, "onStateAND": 130 },
  // ... 4 more
]
```

All 6 are loaded as separate engage rules. Any matching rule triggers engagement.

### vClaim: CAN Address Construction

The `vClaim` value is embedded in CAN IDs for V-Bus messages. The `VSendCurve` and `VFilter` fields already contain the full CAN IDs with vClaim embedded, so the engine doesn't need to construct addresses - it uses the IDs directly from config.

---

## Kickout Detection: Separate from CANProtocolEngine

Kickout detection remains **outside** the CANProtocolEngine for these reasons:

1. **Different detection model**: KickoutMonitor uses local sensors (encoder, pressure, current), not CAN messages
2. **Different state machine**: Kickout has grace periods, cooldown timers, and auto-recovery
3. **Different responsibility**: Kickout = safety stop; Valve Ready = readiness check
4. **Clean separation exists**: KickoutMonitor already handles all kickout logic independently

If CAN-based kickout is needed (like MF valve status transition), it should feed into the existing KickoutMonitor:

```cpp
// In CANProtocolEngine or TractorCANDriver:
if (valveTransitionDetected) {
    kickoutMonitor->triggerCANKickout(KickoutReason::CAN_VALVE_STATUS, valveStatus);
}
```

This keeps the state machine, grace periods, and recovery logic in one place.

---

## Integration with TractorCANDriver

TractorCANDriver continues to own bus management and message routing. The engine handles protocol interpretation:

```cpp
class TractorCANDriver : public MotorDriverInterface {
private:
    CANProtocolEngine protocolEngine;    // NEW: replaces all brand-specific code

    // Keya handling remains separate (different protocol)
    void processKeyaMessage(const CAN_message_t& msg);
    void sendKeyaCommands();

public:
    bool init() override {
        // Load JSON config
        if (CANConfigStorage::hasCustomConfig()) {
            String json = CANConfigStorage::readCustomConfig();
            protocolEngine.loadConfig(json.c_str(), json.length());
        }
        // Setup CAN filters from config
        protocolEngine.setupFilters(steerBusNum, steerCAN);
        protocolEngine.setupFilters(buttonBusNum, buttonCAN);
    }

    void processIncomingMessages() {
        if (brand == TractorBrand::GENERIC) {
            // Keya path (unchanged)
            processKeyaMessage(msg);
        } else {
            // All other brands: use protocol engine
            protocolEngine.processIncomingMessage(busNum, msg);
        }
    }

    void sendSteeringCommands() {
        if (brand == TractorBrand::GENERIC) {
            sendKeyaCommands();
        } else {
            protocolEngine.sendSteerCommand(curve, steerActive, steerCAN);
        }
    }

    // Unified API for AutosteerProcessor
    bool checkEngageEvent() { return protocolEngine.checkEngageEvent(); }
    const char* getEngageLabel() { return protocolEngine.getLastEngageLabel(); }
    bool isValveReady() const { return protocolEngine.isValveReady(); }
};
```

## Simplified AutosteerProcessor

Replace ~140 lines of brand-specific button logic with ~20 lines:

```cpp
// BEFORE: 7 static state variables, 7 edge detection blocks, combined conditional
static bool lastMasseyEngageState = false;
static bool lastFendtButtonState = false;
static bool lastCaseIHEngageState = false;
// ... 4 more

bool masseyEngagePressed = false;
bool fendtButtonPressed = false;
// ... 5 more

// 7 blocks of: read state, detect edge, set flag
// Combined: if (buttonReading || massey || fendt || caseIH || catMT || claas || jcb || lindner)

// AFTER: 1 call
bool canEngagePressed = false;
const char* buttonType = "button";

if (tractorCAN && tractorCAN->checkEngageEvent()) {
    canEngagePressed = true;
    buttonType = tractorCAN->getEngageLabel();
}

if ((buttonReading == LOW && lastButtonReading == HIGH) || canEngagePressed) {
    // Safety check and toggle (unchanged)
    steerState = !steerState;
    LOG_INFO(EventSource::AUTOSTEER, "Autosteer %s via %s",
             steerState == 0 ? "ARMED" : "DISARMED", buttonType);
}
```

---

## File Plan

### New Files

| File | Purpose | Est. Lines |
|------|---------|------------|
| `lib/aio_autosteer/CANProtocolEngine.h` | Class definition, config structs | ~100 |
| `lib/aio_autosteer/CANProtocolEngine.cpp` | Message processing, command sending, state | ~350 |
| `lib/aio_autosteer/CANConfigParser.h` | JSON parsing declarations | ~40 |
| `lib/aio_autosteer/CANConfigParser.cpp` | JSON parsing implementation | ~200 |

### Modified Files

| File | Changes |
|------|---------|
| `TractorCANDriver.h` | Add engine member, remove brand-specific flags/methods (~50 lines removed) |
| `TractorCANDriver.cpp` | Delegate to engine, remove brand process/send functions (~800 lines removed) |
| `AutosteerProcessor.cpp` | Use unified `checkEngageEvent()` API (~120 lines removed) |
| `CANFilterHelper.cpp` | Replace brand-specific filter functions with engine (~250 lines removed) |

### Code Impact

| | Lines |
|---|---|
| Code removed | ~1,220 |
| Code added | ~690 |
| **Net reduction** | **~530** |

---

## JSON Schema Extensions

Minor additions needed for full coverage:

```json
{
  "canConfig": {
    "byteOrder": "little",                  // Default: "little" (big for Fendt/CAT)
    "valveReadyCondition": "nonzero"         // "nonzero" (default), "range:15000:17000" (CAT)
  },
  "steer": [
    {
      "edgeType": "rising",                  // "rising" (default) or "falling"
      "conditions": [                         // Multi-byte (optional, replaces byte/onStateAND)
        { "byte": 0, "mask": 255, "value": 130 },
        { "byte": 1, "mask": 255, "value": 1 }
      ],
      "logic": "AND"
    }
  ]
}
```

All extensions are optional with sensible defaults. Existing JSON configs work unchanged.

---

## Migration Strategy

1. **Create CANProtocolEngine** alongside existing brand-specific code
2. **Feature flag** in TractorCANDriver to switch between old/new systems
3. **Test one brand at a time** - start with Valtra/MF (the PR #7 use case)
4. **Log both systems** simultaneously to verify identical behavior
5. **Enable per-brand** as each is validated
6. **Remove legacy code** once all brands pass validation

---

## Verification Plan

1. **Build**: `~/.platformio/penv/bin/pio run -e teensy41`

2. **Serial debug command**: Dump parsed config for inspection
   ```
   > canconfig
   Engage rules: 2
     [0] CAN 0x18EF1C32 byte 2 mask 0x01 rising "Engage button"
     [1] CAN 0x18EF1CFC byte 3 mask 0xFF rising "McCormick engage"
   Receive: CAN 0x0CAC1C13 curve 0,1 valve 2 LE
   Send: CAN 0x0CAD131C len 8 curve 0,1 intent 2 (253/252) LE
   Filters: 0x0CAC1C13 0x18EF1C32 0x18EF1CFC 0x18EF1C00
   ```

3. **Per-brand testing**: Upload JSON, verify on hardware:
   - Engagement detection matches current behavior
   - Valve ready detection matches
   - Steering commands are byte-identical (verify with CAN analyzer)

4. **Regression**: Run old and new simultaneously, compare log output

---

## Benefits Summary

| Before | After |
|--------|-------|
| Adding brand = 150+ lines of C++ across 4 files | Adding brand = JSON config file only |
| 9 sets of hard-coded CAN IDs | CAN IDs loaded from config |
| Brand logic spread across 4 source files | Single CANProtocolEngine class |
| ~1,220 lines brand-specific code | ~690 lines generic code |
| Firmware rebuild required for brand changes | Config updatable via web UI |
| Brand numbering mismatch (CANFilterHelper vs TractorCANDriver) | Single config source of truth |
