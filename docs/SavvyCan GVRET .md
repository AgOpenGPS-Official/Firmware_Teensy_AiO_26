# GVRET TCP Server Implementation Plan

## Context

De huidige CAN Sniffer web interface heeft PROGMEM beperkingen waardoor de JavaScript niet volledig werkt. In plaats van dit te fixen, kiezen we voor een betere aanpak: gebruik SavvyCAN als CAN analysis tool via GVRET protocol over TCP.

**Voordelen:**
- Geen PROGMEM issues (geen JavaScript in firmware)
- SavvyCAN heeft alle analysis tools (filtering, decoding, graphing, logging)
- Bidiirectioneel - kan CAN frames sturen vanuit SavvyCAN
- Bewezen protocol dat door veel tools wordt ondersteund

**Aanpak:**
- Teensy draait TCP server op poorten 2201, 2202, 2203 (voor CAN1, CAN2, CAN3)
- SavvyCAN verbindt via TCP naar `192.168.5.126:2201` (etc.)
- CAN frames worden in GVRET formaat verstuurd naar SavvyCAN

## Requirements

1. **TCP Server**: 3 servers op poorten 2201, 2202, 2203
2. **GVRET Protocol**: Binary protocol voor CAN frame transmissie
3. **Monitoring**: Alle CAN frames op alle 3 buses doorsturen naar geconnecte clients
4. **Transmission**: CAN frames ontvangen van SavvyCAN en versturen op de CAN bus
5. **Performance**: Kan omgaan met hoge CAN traffic zonder main loop te blokkeren

## Implementation Plan

### File Structure

**Nieuwe bestanden:**
1. `lib/aio_communications/GVRETTCPServer.h` - GVRET server class
2. `lib/aio_communications/GVRETTCPServer.cpp` - Implementatie

**Bestaande bestanden aanpassen:**
1. `src/main.cpp` - GVRET initialisatie en scheduler taak
2. `lib/aio_communications/CANGlobals.h` - Global CAN instances gebruiken

### GVRET Protocol Summary

**CAN Frame Format (naar client):**
```
Byte 0-3:   Timestamp (microseconds, little-endian)
Byte 4-7:   CAN ID (little-endian)
Byte 8:     Flags (extended + data length)
Byte 9-16:  Data bytes (0-8)
Last Byte:  Checksum
```

**Belangrijke Commands:**
| Command | Code | Beschrijving |
|---------|------|--------------|
| Binary Mode | 0xE7 | Schakel naar binary mode |
| Send CAN Frame | 0x00 | Stuur CAN frame (van SavvyCAN) |
| Time Sync | 0x01 | Tijd synchronisatie |
| Get Stats | 0x06 | CAN bus statistieken |
| Get Device Info | 0x07 | Device info string |
| Setup CAN Bus | 0x0C | CAN baud rate instellen |
| Comm Validation | 0x09 | Echo test |

### Class Design: GVRETTCPServer

```cpp
class GVRETTCPServer {
public:
    GVRETTCPServer();
    ~GVRETTCPServer();

    bool begin();           // Start servers op poorten 2201-2203
    void stop();            // Stop alle servers
    void handleClients();   // Call van scheduler (HZ_100)
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // CAN message injectie (wordt aangeroepen bij CAN RX)
    void injectCANMessage(uint8_t busNum, const CAN_message_t& msg);

private:
    EthernetServer _servers[3];  // Ports 2201, 2202, 2203

    // Per-client state voor protocol parsing
    struct GVRETClient {
        EthernetClient client;
        uint32_t lastActivity;
        enum class RXState { WAIT_PREFIX, WAIT_COMMAND, WAIT_DATA };
        RXState rxState;
        uint8_t rxBuffer[256];
        uint16_t rxIndex;
        bool binaryMode;
    };

    std::vector<std::unique_ptr<GVRETClient>> _clients[3];
    bool _enabled = false;

    // Command handlers
    void handleBinaryMode(GVRETClient* client);
    void handleSendCANFrame(uint8_t busNum, GVRETClient* client);
    void handleTimeSync(GVRETClient* client);
    void handleGetCANBusStats(uint8_t busNum, GVRETClient* client);
    void handleGetDeviceInfo(GVRETClient* client);
    void handleSetupCanBus(GVRETClient* client);
    void handleCommValidation(GVRETClient* client);

    // Helpers
    void sendFrameToClients(uint8_t busNum, const CAN_message_t& msg);
    uint8_t calculateChecksum(const uint8_t* data, size_t length);
    void sendResponse(GVRETClient* client, const uint8_t* data, size_t length);
};
```

### Integration in main.cpp

**Include:**
```cpp
#include "GVRETTCPServer.h"
```

**Global instance:**
```cpp
GVRETTCPServer gvretServer;
```

**Initialize in setup():**
```cpp
// Na webManager initialisatie (~ regel 486)
if (gvretServer.begin()) {
    LOG_INFO(EventSource::SYSTEM, "GVRET TCP Server initialized on ports 2201-2203");
} else {
    LOG_ERROR(EventSource::SYSTEM, "GVRET TCP Server FAILED");
}
```

**Add scheduler task (HZ_100):**
```cpp
// ~ regel 535, na web telemetry task
scheduler.addTask(SimpleScheduler::HZ_100, []{
    gvretServer.handleClients();
}, "GVRET Server");
```

### CAN Message Injection

Twee opties:

**Optie A: Polling (Simpler)**
GVRET pollt zelf de CAN buses in `handleClients()`:
```cpp
void GVRETTCPServer::handleClients() {
    // Poll elke CAN bus voor nieuwe messages
    for (uint8_t bus = 0; bus < 3; bus++) {
        CAN_message_t msg;
        while (globalCAN1.read(msg)) {  // of globalCAN2, globalCAN3
            sendFrameToClients(bus, msg);
        }
    }
    // Handle TCP clients...
}
```

**Optie B: Callback (Cleaner)**
Registreer callback in CANGlobals die wordt aangeroepen bij CAN RX.

**Aanbeveling:** Start met Optie A (polling) - simpeler en werkt bestaande infrastructuur.

### Phases

**Phase 1: Basic Server Structure**
- GVRETTCPServer class aanmaken
- EthernetServer initialisatie op poorten 2201-2203
- Client connection/disconnection handling

**Phase 2: CAN Frame Monitoring**
- Poll CAN buses voor messages
- Formatteer volgens GVRET protocol
- Verstuurt naar connected clients

**Phase 3: Bidirectional Communication**
- Command parsing (0xF1 prefix)
- CAN frame transmission (0x00 command)
- Basic responses (stats, device info, validation)

**Phase 4: Testing**
- SavvyCAN verbinding testen
- Frame capture verifiëren
- Frame transmission testen

## Critical Files

| Bestand | Actie |
|---------|-------|
| `lib/aio_communications/GVRETTCPServer.h` | NIEUW - Class definition |
| `lib/aio_communications/GVRETTCPServer.cpp` | NIEUW - Implementatie |
| `src/main.cpp` | AANPASSEN - Init en scheduler |
| `lib/aio_communications/CANGlobals.h` | LEZEN - CAN instances |

## Verification

1. **Connection Test:**
   - SavvyCAN: Connection → New Connection → GVRET_TCP
   - IP: 192.168.5.126, Port: 2201
   - Verify: "Connected" status

2. **Frame Monitoring:**
   - Genereer CAN traffic
   - Verify: Frames appear in SavvyCAN grid
   - Check: Timestamps, IDs, data correct

3. **Frame Transmission:**
   - SavvyCAN: Frame sending → Single frame
   - Verify: Frame appears on CAN bus

4. **Multi-Bus:**
   - Connect naar alle 3 poorten
   - Verify: Elke port toont juiste bus data

## Memory Strategy

- Client state: ~200 bytes per client, max 4 clients per bus = ~2.4KB
- TX buffers: Use DMAMEM (RAM2) voor efficiëntie
- Geen grote buffers - streaming transmission naar TCP

## Notes

- Volgt bestaande patterns van SimpleHTTPServer/SimpleWebSocket
- Geen over-engineering - start minimal,扩展 indien nodig
- Geen web UI changes - GVRET is vervanging voor CAN Sniffer pagina
- bestaande CANSniffer kan blijven voor backward compatibility
