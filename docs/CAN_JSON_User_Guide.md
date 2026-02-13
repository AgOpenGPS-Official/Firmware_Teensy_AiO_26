# CAN Bus JSON Configuration User Guide

This guide explains how to create, customize, and use CAN bus JSON configuration files for AiO v26.

## Table of Contents
1. [Overview](#overview)
2. [How CAN Configs Work (Protocol Engine)](#how-can-configs-work-protocol-engine)
3. [Quick Start](#quick-start)
4. [File Structure Reference](#file-structure-reference)
5. [Creating a Custom Configuration](#creating-a-custom-configuration)
6. [Adding Steer Engage Buttons](#adding-steer-engage-buttons)
7. [Adding Work/Hitch Control](#adding-workhitch-control)
8. [CAN Protocol Settings](#can-protocol-settings)
9. [Discovering CAN Messages](#discovering-can-messages)
10. [Troubleshooting](#troubleshooting)
11. [Examples](#examples)

---

## Overview

### What Are These Files For?

CAN JSON configuration files define how AiO v26 communicates with your tractor's CAN bus. They specify:
- Which CAN messages to listen for (steer button, hitch position)
- Which CAN messages to send (steering commands)
- How to decode button presses and sensor values
- Brand-specific protocol details

### When Do You Need a Custom Config?

- Your tractor brand isn't in the pre-built configs
- Your specific model has different button CAN IDs than existing configs
- You want to add support for additional buttons or sensors
- You're reverse-engineering a new tractor's CAN bus

---

## How CAN Configs Work (Protocol Engine)

### What Changed

AiO v26 includes a **protocol engine** that reads your JSON configuration at boot and uses it to drive all CAN behavior automatically. Instead of requiring firmware changes for each tractor brand, the engine interprets the JSON config to handle:

- Steering valve commands (send/receive)
- Steer engage button detection
- Valve ready status monitoring
- CAN bus hardware filtering

This means you can add support for a new tractor brand or model by uploading a JSON file — no firmware recompilation needed.

### How It Works

1. **Upload** a JSON config via the web UI at `http://192.168.5.126/canupload`
2. The config is **stored on device flash** (LittleFS) and persists across reboots
3. At boot, the firmware **loads the config** and matches it to your selected brand
4. The protocol engine takes over CAN communication for that brand

### What You See in the Serial Log

When the protocol engine loads successfully, you'll see:

```
CANProtocolEngine: Loading config for FENDT (id=4)
CANProtocolEngine: 3 filter IDs parsed
CANProtocolEngine: 2 engage rules loaded, 5 total filter IDs
  Rule[0]: CAN 0x14FF7706 rising 2 conditions "Armrest button"
  Rule[1]: CAN 0x613 falling 1 conditions "Joystick button"
Protocol engine loaded - 2 engage rules, 5 filters
```

- **engage rules** — patterns the engine watches for to detect steer button presses
- **filters** — CAN IDs programmed into hardware mailboxes so only relevant messages are processed

If no config is uploaded, or the config doesn't match your selected brand, you'll see:

```
Protocol engine not loaded - using legacy brand code
```

This is normal — it means the firmware is using its built-in brand support instead.

### Fallback Behavior (Temporary)

During development and testing of the protocol engine, the firmware falls back to legacy hard-coded brand logic when:

- No JSON config has been uploaded
- The uploaded config doesn't contain your selected brand
- The JSON has a parse error (check serial log for details)
- The brand is set to DISABLED
- Keya motor mode is active (Keya uses its own CAN protocol)

This fallback exists so existing users are not disrupted while the engine is validated. Once the protocol engine is fully tested, the hard-coded brand logic will be removed — JSON configs will be the sole mechanism for CAN brand support. That is the point of using JSON: brand support becomes a data problem, not a firmware problem.

### When You DON'T Need a Custom Config

If your tractor brand is already supported in the firmware, it works out of the box without uploading anything. Built-in brands include:

| Brand | Notes |
|-------|-------|
| Case IH / New Holland | |
| CAT MT Series | |
| Claas | |
| Fendt SCR/S4/Gen6 | |
| Fendt One | |
| JCB | |
| Lindner | |
| Valtra / Massey Ferguson | |
| Generic (Keya) | For Keya BLDC motors |

You only need to upload a JSON config when:
- You want to add a **steer engage button** that isn't in the built-in code
- You have a brand or model **not listed above**
- You want to **customize** steering valve parameters for your specific setup

If you do upload a config for a built-in brand, the protocol engine takes over and uses your config instead of the built-in code.

---

## Quick Start

### Using Pre-Built Configurations

1. Connect to AiO at `http://192.168.5.126`
2. Go to **CAN Configuration** page
3. Upload the appropriate JSON file from `CanConfig_Examples/`:
   - Case IH / New Holland → `Case_IH_New_Holland.json`
   - Fendt SCR/S4/Gen6 → `Fendt_SCR_S4_Gen6.json`
   - Claas → `Claas.json`
   - etc.
4. Select your model from the dropdown
5. Select your steer button configuration
6. Save and test

---

## File Structure Reference

### Top-Level Structure

```json
{
  "version": "2.0",
  "metadata": { ... },
  "functions": { ... },
  "busTypes": { ... },
  "brands": [ ... ]
}
```

### Metadata Section

```json
"metadata": {
  "description": "CAN bus configuration for AiO v26",
  "lastUpdated": "2025-01-12",
  "schema": "Supports drag-and-drop UI configuration"
}
```

### Functions Section

Defines the available CAN functions. **Do not modify** - these are fixed by firmware.

```json
"functions": {
  "steering": {
    "name": "Steering",
    "color": "#3498db",
    "description": "Valve/Motor steering control",
    "exclusive": true,      // Only one bus can have steering
    "bitValue": 1           // Internal firmware flag
  },
  "buttons": {
    "name": "Buttons",
    "exclusive": false,     // Multiple buses can have buttons
    "bitValue": 2
  },
  "hitch": { ... },
  "implement": { ... },
  "keya": { ... }
}
```

### Bus Types Section

Defines available CAN bus types. **Do not modify** - matches firmware.

```json
"busTypes": {
  "None": { "id": 0, "displayName": "None" },
  "V_Bus": { "id": 1, "displayName": "V_Bus", "defaultSpeed": 250 },
  "K_Bus": { "id": 2, "displayName": "K_Bus", "defaultSpeed": 500 },
  "ISO_Bus": { "id": 3, "displayName": "ISO_Bus", "defaultSpeed": 250 }
}
```

| Bus Type | Typical Use | Speed |
|----------|-------------|-------|
| V_Bus | Steering valve control | 250 kbps |
| K_Bus | Tractor buttons, sensors | 500 kbps |
| ISO_Bus | ISOBUS implements | 250 kbps |

### Brands Array

This is where you define tractor-specific configurations.

```json
"brands": [
  { "id": 0, "name": "DISABLED", ... },    // REQUIRED - must be first
  { "id": 1, "name": "YOUR_BRAND", ... },  // Your brand configs
  { "id": 6, "name": "GENERIC", ... }      // REQUIRED - must be last
]
```

**Important**: Every config file MUST include `DISABLED` (id: 0) and `GENERIC` (id: 6).

---

## Creating a Custom Configuration

### Step 1: Start with the Template

Copy `Generic_Default.json` as your starting point:

```json
{
  "version": "2.0",
  "metadata": {
    "description": "My custom tractor config",
    "lastUpdated": "2025-01-28"
  },
  "functions": { /* copy from Generic_Default.json */ },
  "busTypes": { /* copy from Generic_Default.json */ },
  "brands": [
    {
      "id": 0,
      "name": "DISABLED",
      "displayName": "Disabled",
      "capabilities": {}
    },
    {
      "id": 7,
      "name": "MY_TRACTOR",
      "displayName": "My Tractor Brand",
      "description": "Custom configuration for my tractor",
      "capabilities": {
        "V_Bus": ["steering"],
        "K_Bus": ["buttons", "hitch"]
      },
      "canConfig": { },
      "models": [ ]
    },
    {
      "id": 6,
      "name": "GENERIC",
      "displayName": "Generic",
      "capabilities": {
        "V_Bus": ["steering"],
        "K_Bus": ["buttons", "hitch"],
        "ISO_Bus": ["steering", "implement"],
        "None": ["keya"]
      },
      "allowsKeya": true
    }
  ]
}
```

### Step 2: Define Capabilities

The `capabilities` object defines which functions can be assigned to which bus:

```json
"capabilities": {
  "V_Bus": ["steering"],           // V_Bus can do steering
  "K_Bus": ["buttons", "hitch"],   // K_Bus can do buttons and hitch
  "ISO_Bus": []                    // ISO_Bus not used for this brand
}
```

### Step 3: Add CAN Protocol Configuration

The `canConfig` object contains brand-specific CAN protocol details:

```json
"canConfig": {
  "VFilter": "0x0CACAA08",         // CAN ID to listen for on V_Bus
  "VSendCurve": "0x0CAD08AA",      // CAN ID to send steering commands
  "SendCurve": "0,0,0,255,255,255,255,255",  // Default message bytes
  "SendCurveLoHi": "0,1",          // Which bytes contain curve value
  "ITS1": "2,253",                 // Inverse Turn Signal byte,value
  "ITS0": "2,252"
}
```

### Step 4: Add Models and Buttons

See the next sections for detailed button and work control configuration.

---

## Adding Steer Engage Buttons

### Button Configuration Structure

```json
"models": [
  {
    "model": "My Tractor Model 2020",
    "vClaim": "AA",                    // V_Bus claim address (hex)
    "notes": "User-visible notes about this model",
    "steer": [
      {
        "buttonLabel": "Armrest steer button",
        "bus": "K_Bus",
        "canFilterID": "0x18FFB306",   // CAN ID to listen for
        "byte": 2,                     // Which byte (0-7)
        "onStateAND": 1                // Bitmask for button pressed
      }
    ]
  }
]
```

### Understanding Button Detection

When a CAN message with ID `0x18FFB306` arrives:
1. Firmware checks byte 2 of the 8-byte payload
2. ANDs it with `onStateAND` value (1 in this case)
3. If result is non-zero, button is pressed

**Example**:
- Byte 2 contains `0x05` (binary: `00000101`)
- `onStateAND` is `1` (binary: `00000001`)
- `0x05 AND 0x01 = 0x01` (non-zero) → Button pressed!

### Multiple Button Options

You can provide multiple button configurations for the same model:

```json
"steer": [
  {
    "buttonLabel": "Joystick button",
    "bus": "K_Bus",
    "canFilterID": "0x18FFB306",
    "byte": 2,
    "onStateAND": 1
  },
  {
    "buttonLabel": "Armrest button (alternative)",
    "bus": "K_Bus",
    "canFilterID": "0x18FFB031",
    "byte": 2,
    "onStateAND": 16
  }
]
```

Users can select which button to use from the web UI dropdown.

---

## Adding Work/Hitch Control

### Work Control Structure

Work control sends CAN messages when entering/exiting a work state (lowering/raising implement).

```json
"work": [
  {
    "inWork": {
      "bus": "K_Bus",
      "label": "Lower Hitch",
      "canFilterID": "0x14204146",
      "send": "0x15,0x20,0x06,0xCA,0x80,0x01,0x00,0x00",
      "operation": "Work"
    },
    "outWork": {
      "bus": "K_Bus",
      "label": "Raise Hitch",
      "canFilterID": "0x14204146",
      "send": "0x15,0x21,0x06,0xCA,0x80,0x03,0x00,0x00",
      "operation": "OutWork"
    }
  }
]
```

### Hitch Height Sensor (Range Type)

For reading hitch position from CAN:

```json
"work": [
  {
    "buttonLabel": "Hitch Height Sensor",
    "bus": "K_Bus",
    "canFilterID": "0x18FE4523",
    "bits": "9-16",
    "operation": "Work",
    "type": "Range"
  }
]
```

---

## CAN Protocol Settings

### Steering Valve Protocol (canConfig)

| Field | Description | Example |
|-------|-------------|---------|
| `VFilter` | CAN IDs to receive on V_Bus (comma-separated) | `"0x0CACAA08,0x18FFBB03"` |
| `VReceiveCurve` | CAN ID for receiving curve/valve status | `"0x0CAC1C13"` |
| `ReceiveCurveLoHi` | Byte positions for received curve value (low,high) | `"0,1"` |
| `ValveState` | Byte position for valve ready status (0 = not ready) | `2` |
| `CurveScale` | Divisor to convert raw CAN curve to degrees (default 100) | `100` |
| `VSendCurve` | CAN ID for sending steering commands | `"0x0CAD08AA"` |
| `VDataLen` | Data length for V_Bus messages | `6` or `8` |
| `SendCurve` | Default byte values for curve message | `"0,0,0,255,255,255,255,255"` |
| `SendCurveLoHi` | Byte positions for curve value (low,high) | `"0,1"` |
| `SendCurveSteer` | Byte values when steering active | `"5,9,3,10,0,0"` |
| `SendCurveNoSteer` | Byte values when steering inactive | `"5,9,2,10,0,0"` |
| `CurveSteerLoHi` | Curve byte positions for steer mode | `"4,5"` |
| `curveMod` | Curve value modifier | `-32128` |
| `ITS1` | Inverse Turn Signal ON: byte,value | `"2,253"` |
| `ITS0` | Inverse Turn Signal OFF: byte,value | `"2,252"` |

### V_Bus Claim Address

The `vClaim` field is the address your AiO claims on the V_Bus:

```json
"vClaim": "2C"   // Hex value, commonly "AA" or "2C"
```

This must be unique on the bus and may need to match what your tractor expects.

---

## Discovering CAN Messages

### What You Need

1. **CAN bus sniffer/analyzer** - Tools like:
   - USB-CAN adapter + software (CANalyzer, SavvyCAN)
   - Dedicated CAN logger
   - AiO's built-in CAN monitor (if available)

2. **Access to tractor CAN bus** - Usually via:
   - Diagnostic port
   - ISO 11783 connector (ISOBUS)
   - Rear pillar connector
   - Splice into existing wiring

### Finding Button CAN IDs

1. **Start logging** all CAN traffic
2. **Press the button** you want to use
3. **Look for changes** - Find messages that only appear or change when button is pressed
4. **Identify the pattern**:
   - Note the CAN ID (e.g., `0x18FFB306`)
   - Note which byte changes
   - Note the value when pressed vs released

### Example Discovery Session

```
Before button press:
ID: 0x18FFB306  Data: 00 00 00 00 00 00 00 00

During button press:
ID: 0x18FFB306  Data: 00 00 01 00 00 00 00 00
                           ^^-- Byte 2 changed to 0x01

After button release:
ID: 0x18FFB306  Data: 00 00 00 00 00 00 00 00
```

Configuration:
```json
{
  "canFilterID": "0x18FFB306",
  "byte": 2,
  "onStateAND": 1
}
```

### Tips for CAN Discovery

- **Filter by changes**: Most sniffers can highlight changed data
- **Look for patterns**: Buttons often use bit flags, not whole bytes
- **Check multiple presses**: Ensure it's consistent
- **Try different buttons**: Some tractors have multiple usable buttons
- **Document everything**: CAN IDs can vary by model year and options

---

## Troubleshooting

### Button Not Working

1. **Verify CAN ID**: Double-check the hex ID is correct
2. **Check bus assignment**: Is the button on K_Bus or V_Bus?
3. **Verify byte position**: Bytes are numbered 0-7
4. **Check AND mask**: Try different bit values (1, 2, 4, 8, 16, 32, 64, 128)
5. **Monitor CAN traffic**: Use AiO's CAN debug page to see incoming messages

### Steering Not Responding

1. **V_Bus connected?**: Check physical CAN connection
2. **Correct speed?**: V_Bus is typically 250 kbps
3. **Claim address conflict?**: Another device may be using the same address
4. **VFilter correct?**: Must match what your valve controller sends

### JSON Parse Errors

1. **Validate JSON**: Use a JSON validator (jsonlint.com)
2. **Check commas**: No trailing commas in arrays/objects
3. **Check quotes**: All strings must be double-quoted
4. **Check hex format**: Use `"0x1234"` (string with 0x prefix)

### Common Mistakes

| Mistake | Correct |
|---------|---------|
| `0x18FFB306` (no quotes) | `"0x18FFB306"` (quoted string) |
| `"byte": "2"` (string) | `"byte": 2` (number) |
| Trailing comma `[1, 2, ]` | No trailing comma `[1, 2]` |
| Missing DISABLED brand | Must include id:0 DISABLED |
| Missing GENERIC brand | Must include id:6 GENERIC |

---

## Examples

### Minimal Custom Brand

```json
{
  "id": 7,
  "name": "MY_TRACTOR",
  "displayName": "My Tractor",
  "capabilities": {
    "V_Bus": ["steering"],
    "K_Bus": ["buttons"]
  },
  "models": [
    {
      "model": "Model X",
      "vClaim": "AA",
      "steer": [
        {
          "buttonLabel": "Armrest button",
          "bus": "K_Bus",
          "canFilterID": "0x18FF1234",
          "byte": 0,
          "onStateAND": 1
        }
      ]
    }
  ]
}
```

### Multiple Models, Same Brand

```json
{
  "id": 7,
  "name": "MY_BRAND",
  "displayName": "My Brand",
  "capabilities": {
    "V_Bus": ["steering"],
    "K_Bus": ["buttons"]
  },
  "models": [
    {
      "model": "Small Tractor 2020",
      "vClaim": "AA",
      "steer": [
        { "buttonLabel": "Button A", "bus": "K_Bus", "canFilterID": "0x18FF1111", "byte": 0, "onStateAND": 1 }
      ]
    },
    {
      "model": "Big Tractor 2022",
      "vClaim": "2C",
      "steer": [
        { "buttonLabel": "Button B", "bus": "K_Bus", "canFilterID": "0x18FF2222", "byte": 1, "onStateAND": 4 }
      ]
    }
  ]
}
```

### Brand with Full CAN Config

```json
{
  "id": 7,
  "name": "FULL_EXAMPLE",
  "displayName": "Full Example Brand",
  "capabilities": {
    "V_Bus": ["steering"],
    "K_Bus": ["buttons", "hitch"]
  },
  "canConfig": {
    "VFilter": "0x0CEF2CF0",
    "curveMod": -32128,
    "VSendCurve": "0x0CEFF02C",
    "VDataLen": 6,
    "SendCurveSteer": "5,9,3,10,0,0",
    "SendCurveNoSteer": "5,9,2,10,0,0",
    "CurveSteerLoHi": "4,5"
  },
  "models": [
    {
      "model": "Example Model",
      "vClaim": "2C",
      "steer": [
        {
          "buttonLabel": "Steering engage button",
          "bus": "K_Bus",
          "canFilterID": "0x613",
          "byte": 1,
          "onStateAND": 4
        }
      ],
      "work": [
        {
          "inWork": {
            "bus": "K_Bus",
            "label": "Start Work",
            "canFilterID": "0x14204146",
            "send": "0x15,0x20,0x06,0xCA,0x80,0x01,0x00,0x00"
          },
          "outWork": {
            "bus": "K_Bus",
            "label": "End Work",
            "canFilterID": "0x14204146",
            "send": "0x15,0x21,0x06,0xCA,0x80,0x03,0x00,0x00"
          }
        }
      ]
    }
  ]
}
```

---

## Getting Help

- **AgOpenGPS Forum**: Share your config and get help from the community
- **GitHub Issues**: Report bugs or request features
- **Existing Configs**: Study the pre-built configs in `CanConfig_Examples/` for your brand's "neighbors"

## Contributing

If you create a working configuration for a new tractor brand or model:
1. Test it thoroughly
2. Document the model years/variants it works with
3. Submit it to the project for inclusion in `CanConfig_Examples/`

---

*Last updated: 2025-01-28*
