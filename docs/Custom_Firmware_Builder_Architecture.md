# Custom Firmware Builder Architecture

## Problem

The AiO v26 firmware currently compiles as a single monolithic binary with all tractor brands, motor drivers, and features included. This leads to proliferation of custom firmware forks in the community — users strip out features they don't need, add tractor-specific tweaks, and the forks diverge from upstream. Updates become painful and community knowledge fragments across forks.

## Solution

A **web-based custom firmware builder** where users:
1. Visit a website and select their hardware and tractor brand
2. A build server compiles firmware with only the selected components
3. Users download the `.hex` file and flash via the existing OTA web interface

This keeps **one canonical codebase** that everyone contributes to, while users get minimal firmware tailored to their setup.

## Design Principles

- **One class per tractor brand** — Each brand is a self-contained file. Community members submit PRs to update their brand's class. No JSON configuration approach needed.
- **Default-ON / Opt-OUT** — A standard `pio run` (no flags) produces the full firmware, identical to today. The builder passes `AIO_EXCLUDE_*` flags to remove unchecked components. This preserves backward compatibility.
- **Stub classes** — Excluded modules compile to empty stubs (no-op methods). This avoids scattering `#ifdef` at every call site throughout the codebase.
- **GitHub Actions as build server** — No external infrastructure needed. The existing CI/CD pipeline is extended with a parameterized workflow.

---

## Architecture Overview

```
┌─────────────────────────────────────┐
│         Frontend Website            │
│   (Static HTML/JS on GitHub Pages)  │
│                                     │
│  ┌─────────┐ ┌──────────────────┐   │
│  │ Motor   │ │ Tractor Brand    │   │
│  │ Type    │ │ Selection        │   │
│  │ ○ PWM   │ │ ☑ Fendt          │   │
│  │ ○ Keya  │ │ ☑ Valtra/MF      │   │
│  │ ○ Danf  │ │ ☐ Case IH        │   │
│  └─────────┘ │ ☐ Claas          │   │
│              └──────────────────┘   │
│  ┌──────────────────────────────┐   │
│  │ Optional Features            │   │
│  │ ☑ Section Control            │   │
│  │ ☑ LED Status  ☐ ESP32 Bridge │   │
│  └──────────────────────────────┘   │
│         [ Build My Firmware ]       │
└──────────────┬──────────────────────┘
               │ API call
               ▼
┌─────────────────────────────────────┐
│  Cloudflare Worker (API Proxy)      │
│  Holds GitHub PAT, proxies request  │
└──────────────┬──────────────────────┘
               │ workflow_dispatch
               ▼
┌─────────────────────────────────────┐
│  GitHub Actions: custom-build.yml   │
│                                     │
│  1. Convert selections → -D flags   │
│  2. EXTRA_BUILD_FLAGS="-D AIO_..."  │
│  3. pio run -e teensy41             │
│  4. Upload .hex artifact            │
└──────────────┬──────────────────────┘
               │ download link
               ▼
┌─────────────────────────────────────┐
│  User downloads .hex file           │
│  Uploads to device via /ota page    │
└─────────────────────────────────────┘
```

---

## Part 1: Conditional Compilation

### Feature Flags

All flags follow the pattern `AIO_EXCLUDE_<CATEGORY>_<COMPONENT>`. When the flag is **not** defined, the component is included (default behavior).

#### Motor Drivers
| Flag | Component | Description |
|------|-----------|-------------|
| `AIO_EXCLUDE_MOTOR_PWM` | PWMMotorDriver | Cytron, IBT-2, DRV8701 motor control |
| `AIO_EXCLUDE_MOTOR_KEYA_SERIAL` | KeyaSerialDriver | Keya motor via RS485 |
| `AIO_EXCLUDE_MOTOR_DANFOSS` | DanfossMotorDriver | Danfoss hydraulic valve |
| `AIO_EXCLUDE_MOTOR_TRACTOR_CAN` | TractorCANDriver | All CAN-based tractor steering |

#### Tractor Brands (only when Tractor CAN is included)
| Flag | Brand |
|------|-------|
| `AIO_EXCLUDE_BRAND_FENDT` | Fendt SCR/S4/Gen6 and Fendt One |
| `AIO_EXCLUDE_BRAND_VALTRA_MF` | Valtra and Massey Ferguson |
| `AIO_EXCLUDE_BRAND_CASEIH_NH` | Case IH and New Holland |
| `AIO_EXCLUDE_BRAND_CLAAS` | CLAAS |
| `AIO_EXCLUDE_BRAND_JCB` | JCB |
| `AIO_EXCLUDE_BRAND_LINDNER` | Lindner |
| `AIO_EXCLUDE_BRAND_CAT_MT` | CAT MT Series |
| `AIO_EXCLUDE_BRAND_KEYA_CAN` | Keya CAN motor |

#### Optional Features
| Flag | Component | Description |
|------|-----------|-------------|
| `AIO_EXCLUDE_MACHINE_CONTROL` | MachineProcessor | Section control, relay outputs |
| `AIO_EXCLUDE_ESP32_BRIDGE` | ESP32Interface | WiFi bridge support |
| `AIO_EXCLUDE_LED_MANAGER` | LEDManagerFSM | Status LED indicators |
| `AIO_EXCLUDE_SERIAL_DEBUG` | CommandHandler | Serial debug menu |
| `AIO_EXCLUDE_ENCODER` | EncoderProcessor | Wheel encoder support |
| `AIO_EXCLUDE_PROTOCOL_ENGINE` | CANProtocolEngine | JSON CAN protocol engine |
| `AIO_EXCLUDE_WEB_CAN_CONFIG` | CAN config pages | Web UI for CAN config upload (~88KB savings) |

### Stub Class Pattern

When a module is excluded, its header provides a stub class with no-op methods:

```cpp
// ESP32Interface.h
#ifndef AIO_EXCLUDE_ESP32_BRIDGE

class ESP32Interface {
    // ... full implementation ...
    void init();
    void process();
    bool isDetected() const;
};

#else

// Stub: ESP32 support excluded from this build
class ESP32Interface {
public:
    void init() {}
    void process() {}
    bool isDetected() const { return false; }
};

#endif
```

**Why this approach?** Code that calls `esp32Interface.process()` compiles and links without any `#ifdef` at the call site. The compiler optimizes the empty methods to nothing. This keeps the codebase clean and avoids an explosion of conditional blocks throughout `main.cpp` and other files.

### Dependency Validation

A new `BuildConfig.h` header enforces valid combinations at compile time:

```cpp
// Error if a brand is included but TractorCANDriver is excluded
#if defined(AIO_EXCLUDE_MOTOR_TRACTOR_CAN) && !defined(AIO_EXCLUDE_BRAND_FENDT)
  #error "BRAND_FENDT requires MOTOR_TRACTOR_CAN"
#endif

// Error if Danfoss is included but MachineProcessor is excluded
#if !defined(AIO_EXCLUDE_MOTOR_DANFOSS) && defined(AIO_EXCLUDE_MACHINE_CONTROL)
  #error "MOTOR_DANFOSS requires MACHINE_CONTROL (uses PCA9685)"
#endif
```

---

## Part 2: Per-Brand Tractor Classes

### Current State

All 9 tractor brands are implemented as methods inside `TractorCANDriver.cpp` (~1,085 lines). Brand selection uses switch statements. Adding or modifying a brand requires editing this monolithic file — risky and hard to review.

### New Architecture: Brand Handler Pattern

Each brand becomes a separate file that provides a `BrandHandler` struct with function pointers:

```cpp
// BrandHandler.h
struct BrandHandler {
    const char* name;          // "Fendt SCR/S4/Gen6"
    uint8_t brandId;           // Enum value matching TractorBrand

    // Process incoming CAN message on steer bus
    bool (*processSteerMessage)(TractorCANDriver* driver, const CAN_message_t& msg);

    // Process incoming CAN message on K-Bus/button bus
    bool (*processButtonMessage)(TractorCANDriver* driver, const CAN_message_t& msg);

    // Send steering command (called at 50Hz when enabled)
    void (*sendSteerCommand)(TractorCANDriver* driver, int16_t pwm,
                             bool enabled, bool ready, uint8_t busNum);

    // Check engage state
    bool (*isEngaged)(const TractorCANDriver* driver);

    // Optional brand-specific init
    void (*init)(TractorCANDriver* driver);
};
```

### Brand File Layout

New directory: `lib/aio_autosteer/brands/`

| File | Brand | Extracted From |
|------|-------|---------------|
| `BrandFendt.h/cpp` | Fendt SCR/S4/Gen6 + One | `processFendtMessage`, `sendFendtCommands` |
| `BrandCaseIH.h/cpp` | Case IH / New Holland | `processCaseIHMessage`, `sendCaseIHCommands` |
| `BrandCATMT.h/cpp` | CAT MT Series | `processCATMessage`, `sendCATCommands` |
| `BrandClaas.h/cpp` | CLAAS | `processClaasMessage`, `sendClaasCommands` |
| `BrandJCB.h/cpp` | JCB | `processJcbMessage`, `sendJcbCommands` |
| `BrandLindner.h/cpp` | Lindner | `processLindnerMessage`, `sendLindnerCommands` |
| `BrandValtraMF.h/cpp` | Valtra / Massey Ferguson | `processValtraMessage`, `sendValtraCommands` |
| `BrandKeya.h/cpp` | Keya CAN motor | `processKeyaMessage`, `sendKeyaCommands` |

Each file is ~90-150 lines and self-contained. The entire `.cpp` is wrapped in `#ifndef AIO_EXCLUDE_BRAND_*`, so excluded brands compile to nothing.

### Registration

`TractorCANDriver` maintains a brand registry. During `init()`:

```cpp
void TractorCANDriver::registerAllBrands() {
#ifndef AIO_EXCLUDE_BRAND_FENDT
    registerBrand(&fendtHandler);
    registerBrand(&fendtOneHandler);
#endif
#ifndef AIO_EXCLUDE_BRAND_CASEIH_NH
    registerBrand(&caseIHHandler);
#endif
#ifndef AIO_EXCLUDE_BRAND_CLAAS
    registerBrand(&claasHandler);
#endif
    // ... one block per brand ...
}
```

At runtime, `TractorCANDriver` looks up the active brand's handler by ID and dispatches through the function pointers. The switch statements in `processIncomingMessages()` and `sendSteerCommands()` are replaced by a single handler lookup.

### Benefits for Contributors

- **Adding a new brand**: Create one `.h` and one `.cpp` file, add a `registerBrand()` call
- **Updating a brand**: Edit only that brand's file — no risk of breaking other brands
- **PRs are small and focused**: Reviewers only need to understand one brand's protocol
- **Build-time exclusion**: Users who don't need a brand pay zero code size cost

---

## Part 3: Build System Changes

### platformio.ini

Add one line to pass custom build flags from the environment:

```ini
build_flags =
    -D SCHEDULER_TIMING_STATS
    -Os
    -fno-exceptions
    -fno-rtti
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    ${sysenv.EXTRA_BUILD_FLAGS}
```

When `EXTRA_BUILD_FLAGS` is empty (normal local development), nothing changes. When set by GitHub Actions, PlatformIO appends the exclusion defines.

### Build Manifest

The builder passes a human-readable manifest string:

```
-D AIO_CUSTOM_BUILD -D 'AIO_BUILD_MANIFEST="PWM+Fendt+Sections"'
```

This is displayed at boot and in the web UI:

```
=== Teensy 4.1 - AiO v26.4 (Custom: PWM+Fendt+Sections) ===
```

Standard builds show `(Full)` instead.

### Hex File Naming

Custom builds get descriptive filenames:
- Standard: `AiO_v26.4.hex`
- Custom: `AiO_v26.4_custom_PWM-Fendt-Sections.hex`

---

## Part 4: GitHub Actions Workflow

### New Workflow: `.github/workflows/custom-build.yml`

Triggered by `workflow_dispatch` with boolean inputs for every feature:

```yaml
name: Custom Firmware Build

on:
  workflow_dispatch:
    inputs:
      motor_pwm:         { type: boolean, default: true, description: 'PWM Motor (Cytron/IBT-2)' }
      motor_tractor_can: { type: boolean, default: true, description: 'Tractor CAN Driver' }
      brand_fendt:       { type: boolean, default: true, description: 'Fendt' }
      brand_valtra_mf:   { type: boolean, default: true, description: 'Valtra/Massey Ferguson' }
      brand_caseih_nh:   { type: boolean, default: true, description: 'Case IH/New Holland' }
      machine_control:   { type: boolean, default: true, description: 'Section Control' }
      # ... etc for all toggleable features ...
```

The workflow:
1. Converts unchecked inputs to `-D AIO_EXCLUDE_*` flags
2. Builds a manifest string from checked items
3. Sets `EXTRA_BUILD_FLAGS` environment variable
4. Runs `pio run -e teensy41`
5. Uploads the `.hex` as a downloadable artifact (90-day retention)

This workflow is also callable via the GitHub REST API:
```
POST /repos/{owner}/{repo}/actions/workflows/custom-build.yml/dispatches
{
  "ref": "main",
  "inputs": { "brand_fendt": "true", "brand_claas": "false", ... }
}
```

---

## Part 5: Frontend Website

### Technology Choice

**Static HTML/CSS/JavaScript** — a single `index.html` file. No build tools, no framework, no npm. Hosted on GitHub Pages (free). Agricultural users need it fast, simple, and reliable.

### Location

`docs/builder/index.html` in the firmware repository, deployed via GitHub Pages at a URL like `https://agopengps-official.github.io/Firmware_Teensy_AiO_26/builder/`.

### User Interface

The page has these sections:

1. **Motor Driver** — Radio buttons (pick one):
   - PWM Motor (Cytron/IBT-2/DRV8701) — default
   - Keya Serial Motor
   - Danfoss Hydraulic
   - Tractor CAN Steering → reveals brand selection

2. **Tractor Brand** — Checkboxes (shown when Tractor CAN selected):
   - Fendt, Valtra/MF, Case IH/NH, CLAAS, JCB, Lindner, CAT MT, Keya CAN

3. **Optional Features** — Checkboxes (all on by default):
   - Section Control, LED Status, ESP32 Bridge, Wheel Encoder, Serial Debug

4. **Build Summary** — Real-time preview of what will be compiled

5. **Presets** — One-click common setups:
   - "Full Featured" (everything on)
   - "Fendt + PWM" (typical Fendt user)
   - "Valtra V-Bus" (Valtra/MF with V-Bus engage)
   - "Minimal PWM" (just motor control, no CAN)

6. **Build Button** → triggers the build → shows progress → download link

### API Authentication

A **Cloudflare Worker** (~20 lines of code) proxies the GitHub Actions API call and securely holds the GitHub Personal Access Token. The static website calls the worker, which triggers the build. This avoids exposing the token in client-side JavaScript. Cloudflare Workers' free tier supports 100K requests/day — more than sufficient.

### User Flow

```
1. User visits website
2. Selects motor type, tractor brand, features
3. Clicks "Build My Firmware"
4. Website calls Cloudflare Worker → Worker triggers GitHub Actions
5. Website polls workflow status every 10 seconds
6. When build completes, "Download Firmware" button appears
7. User downloads .hex file
8. User uploads .hex to their device via http://<device-ip>/ota
```

---

## Part 6: Implementation Phases

### Phase 1: Build Infrastructure (Foundation)
- Create `BuildConfig.h` with feature defines and dependency validation
- Add stub classes to optional modules (ESP32, LED, Machine, Encoder, CommandHandler)
- Add `#ifdef` guards to `MotorDriverManager.cpp` factory
- Add `#ifdef` guards to `SimpleWebManager.cpp` for CAN config pages
- Add `${sysenv.EXTRA_BUILD_FLAGS}` to `platformio.ini`
- **Verify**: full build unchanged; builds with various exclusion flags compile clean

### Phase 2: Brand Extraction (Biggest Refactor)
- Create `BrandHandler.h` interface
- Create `lib/aio_autosteer/brands/` directory
- Extract 8 brands into individual files
- Refactor `TractorCANDriver.cpp` to use brand handler registry
- Simplify `AutosteerProcessor.cpp` engage detection (use unified path)
- **Verify**: each brand individually, various combinations, full build

### Phase 3: GitHub Actions Workflow
- Create `custom-build.yml` with parameterized inputs
- Enhance `copy_hex.py` for custom build naming
- Add build manifest display to boot log and web UI
- **Verify**: trigger custom build manually, download artifact, flash via OTA

### Phase 4: Frontend Website
- Create `docs/builder/index.html`
- Set up Cloudflare Worker for API proxy
- Enable GitHub Pages deployment
- **Verify**: end-to-end build, download, and flash

### Phase 5: Polish
- Preset configurations
- Estimated flash savings display
- Shareable configuration URLs (query parameters)
- Contributor documentation: "How to Add a Tractor Brand"

---

## Current Firmware Stats (for reference)

| Metric | Value |
|--------|-------|
| Flash used | 566KB of 8MB (7%) |
| RAM used | 179KB of 1MB (17%) |
| Custom code | ~31,000 lines across 5 libraries |
| Web pages | 288KB PROGMEM (21 files) |
| Tractor brands | 9 (in one 1,085-line file) |
| Motor drivers | 4 types |
| Build output | ~1.5MB Intel HEX |
| Existing conditional compilation | Nearly none |

## Files Created/Modified

| File | Action |
|------|--------|
| `lib/aio_config/BuildConfig.h` | CREATE — feature defines, validation |
| `lib/aio_autosteer/BrandHandler.h` | CREATE — brand handler interface |
| `lib/aio_autosteer/brands/BrandFendt.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandCaseIH.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandCATMT.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandClaas.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandJCB.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandLindner.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandValtraMF.h/cpp` | CREATE |
| `lib/aio_autosteer/brands/BrandKeya.h/cpp` | CREATE |
| `lib/aio_autosteer/TractorCANDriver.h/cpp` | MODIFY — brand registry, state accessors |
| `lib/aio_autosteer/MotorDriverManager.cpp` | MODIFY — `#ifdef` in factory |
| `lib/aio_autosteer/AutosteerProcessor.cpp` | MODIFY — unified engage detection |
| `src/main.cpp` | MODIFY — conditional init (minor due to stubs) |
| `lib/aio_system/SimpleWebManager.cpp` | MODIFY — `#ifdef` CAN config pages |
| `lib/aio_system/ESP32Interface.h` | MODIFY — add stub class |
| `lib/aio_system/LEDManagerFSM.h` | MODIFY — add stub class |
| `lib/aio_system/MachineProcessor.h` | MODIFY — add stub class |
| `lib/aio_autosteer/EncoderProcessor.h` | MODIFY — add stub class |
| `lib/aio_system/CommandHandler.h` | MODIFY — add stub class |
| `platformio.ini` | MODIFY — add env var passthrough |
| `copy_hex.py` | MODIFY — custom build naming |
| `.github/workflows/custom-build.yml` | CREATE — parameterized workflow |
| `docs/builder/index.html` | CREATE — frontend website |
