// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// QNEthernetUDPHandler.cpp
// Implementation of UDP handling using QNEthernet's native EthernetUDP

#include "QNEthernetUDPHandler.h"
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "EventLogger.h"
#include "DHCPLite.h"
#include "ConfigManager.h"
#include "ESP32Interface.h"

#include <string.h>

using namespace qindesign::network;

namespace {
constexpr uint16_t kMDNSPort = 5353;
const IPAddress kMDNSMulticastIP(224, 0, 0, 251);

bool decodeDnsName(const uint8_t* data, size_t len, size_t offset,
                   char* outName, size_t outNameSize, size_t* nextOffset) {
    if (!data || !outName || outNameSize == 0 || offset >= len) {
        return false;
    }

    size_t pos = offset;
    size_t outPos = 0;
    bool first = true;

    while (pos < len) {
        uint8_t labelLen = data[pos++];
        if (labelLen == 0) {
            outName[outPos] = '\0';
            if (nextOffset) {
                *nextOffset = pos;
            }
            return true;
        }

        // Compression is not expected in the incoming question section here.
        if ((labelLen & 0xC0) != 0) {
            return false;
        }

        if (pos + labelLen > len) {
            return false;
        }

        if (!first) {
            if (outPos + 1 >= outNameSize) {
                return false;
            }
            outName[outPos++] = '.';
        }
        first = false;

        for (uint8_t i = 0; i < labelLen; i++) {
            if (outPos + 1 >= outNameSize) {
                return false;
            }
            outName[outPos++] = (char)tolower(data[pos++]);
        }
    }

    return false;
}
}  // namespace

// Static member definitions
EthernetUDP QNEthernetUDPHandler::udpPGN;
EthernetUDP QNEthernetUDPHandler::udpRTCM;
EthernetUDP QNEthernetUDPHandler::udpDHCP;
EthernetUDP QNEthernetUDPHandler::udpDNS;
EthernetUDP QNEthernetUDPHandler::udpMDNS;
EthernetUDP QNEthernetUDPHandler::udpSend;
bool QNEthernetUDPHandler::dhcpServerEnabled = false;
uint8_t QNEthernetUDPHandler::packetBuffer[512];

// External ConfigManager
extern ConfigManager configManager;

void QNEthernetUDPHandler::init() {
    LOG_INFO(EventSource::NETWORK, "Initializing QNEthernet UDP handlers");
    
    // Check Ethernet link status first
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "No Ethernet link detected!");
        return;
    }
    
    // Log network configuration
    IPAddress localIP = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Local IP: %d.%d.%d.%d", 
             localIP[0], localIP[1], localIP[2], localIP[3]);
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    LOG_INFO(EventSource::NETWORK, "Broadcast IP: %d.%d.%d.%d", 
             destIP[0], destIP[1], destIP[2], destIP[3]);
    LOG_INFO(EventSource::NETWORK, "Link Speed: %d Mbps, Full Duplex: %s", 
             Ethernet.linkSpeed(), Ethernet.linkIsFullDuplex() ? "Yes" : "No");
    
    // Set up PGN listener on port 8888 (AgIO sends PGNs to this port)
    if (udpPGN.begin(8888)) {
        LOG_INFO(EventSource::NETWORK, "UDP listening on port 8888 for PGN from AgIO");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start UDP on port 8888");
    }
    
    // Add delay between UDP listeners to avoid conflicts
    delay(100);
    
    // Set up RTCM listener on port 2233
    if (udpRTCM.begin(2233)) {
        LOG_INFO(EventSource::NETWORK, "UDP listening on port 2233 for RTCM");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start UDP on port 2233");
    }
    
    // Add delay between UDP listeners
    delay(100);
    
    // Initialize send socket (no specific port binding needed)
    if (udpSend.begin(0)) {  // 0 = let system choose port
        LOG_INFO(EventSource::NETWORK, "UDP send socket initialized");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to initialize UDP send socket");
    }
    
    // Enable DHCP server by default
    enableDHCPServer(true);
    
    LOG_INFO(EventSource::NETWORK, "QNEthernet UDP initialization complete");
}

void QNEthernetUDPHandler::poll() {
    static uint32_t lastStatusCheck = 0;
    static bool lastLinkStatus = false;
    static uint8_t pollCounter = 0;

    // Skip every other poll to reduce overhead
    pollCounter++;
    if (pollCounter & 1) return;
    
    // Check for incoming PGN packets
    int packetSize = udpPGN.parsePacket();
    if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
        int bytesRead = udpPGN.read(packetBuffer, packetSize);
        if (bytesRead > 0) {
            // Process the packet
            handlePGNPacket(packetBuffer, bytesRead, udpPGN.remoteIP(), udpPGN.remotePort());
        }
    }
    
    // Check for incoming RTCM packets
    packetSize = udpRTCM.parsePacket();
    if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
        int bytesRead = udpRTCM.read(packetBuffer, packetSize);
        if (bytesRead > 0) {
            handleRTCMPacket(packetBuffer, bytesRead, udpRTCM.remoteIP(), udpRTCM.remotePort());
        }
    }
    
    // Check for incoming DHCP packets if server is enabled
    if (dhcpServerEnabled) {
        packetSize = udpDHCP.parsePacket();
        if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
            int bytesRead = udpDHCP.read(packetBuffer, packetSize);
            if (bytesRead > 0) {
                handleDHCPPacket(packetBuffer, bytesRead, udpDHCP.remoteIP(), udpDHCP.remotePort());
            }
        }

        // Check for incoming DNS packets
        packetSize = udpDNS.parsePacket();
        if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
            int bytesRead = udpDNS.read(packetBuffer, packetSize);
            if (bytesRead > 0) {
                static uint32_t lastDNSLog = 0;
                if (millis() - lastDNSLog > 2000) {  // Log every 2 sec
                    lastDNSLog = millis();
                    LOG_DEBUG(EventSource::NETWORK, "DNS packet received: %d bytes from %d.%d.%d.%d port %d",
                             bytesRead, udpDNS.remoteIP()[0], udpDNS.remoteIP()[1], 
                             udpDNS.remoteIP()[2], udpDNS.remoteIP()[3], udpDNS.remotePort());
                }
                handleDNSPacket(packetBuffer, bytesRead, udpDNS.remoteIP(), udpDNS.remotePort());
            }
        }

        // Check for incoming mDNS packets (*.local via multicast)
        packetSize = udpMDNS.parsePacket();
        if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
            int bytesRead = udpMDNS.read(packetBuffer, packetSize);
            if (bytesRead > 0) {
                static uint32_t lastMDNSPacketLog = 0;
                if (millis() - lastMDNSPacketLog > 2000) {  // Log every 2 sec
                    lastMDNSPacketLog = millis();
                    LOG_DEBUG(EventSource::NETWORK, "mDNS packet received: %d bytes from %d.%d.%d.%d port %d",
                             bytesRead, udpMDNS.remoteIP()[0], udpMDNS.remoteIP()[1], 
                             udpMDNS.remoteIP()[2], udpMDNS.remoteIP()[3], udpMDNS.remotePort());
                }
                handleMDNSPacket(packetBuffer, bytesRead, udpMDNS.remoteIP(), udpMDNS.remotePort());
            }
        }
    }
    
    // Check link status every 5 seconds
    if (millis() - lastStatusCheck > 5000) {
        lastStatusCheck = millis();
        
        bool currentLinkStatus = Ethernet.linkState();
        
        // Log if link status changed
        if (currentLinkStatus != lastLinkStatus) {
            lastLinkStatus = currentLinkStatus;
            
            if (currentLinkStatus) {
                IPAddress localIP = Ethernet.localIP();
                LOG_INFO(EventSource::NETWORK, "Ethernet link UP - IP: %d.%d.%d.%d, Speed: %d Mbps", 
                         localIP[0], localIP[1], localIP[2], localIP[3], Ethernet.linkSpeed());
            } else {
                LOG_ERROR(EventSource::NETWORK, "Ethernet link DOWN!");
            }
        }
        
        // Network status now logged by centralized coordinator in main.cpp
    }
}

void QNEthernetUDPHandler::handlePGNPacket(const uint8_t* data, size_t len, 
                                           const IPAddress& remoteIP, uint16_t remotePort) {
    // Process PGN packet
    
    // Forward to ESP32 if detected
    if (esp32Interface.isDetected()) {
        esp32Interface.sendToESP32(data, len);
    }
    
    // Process the packet normally
    if (len > 0 && PGNProcessor::instance) {
        PGNProcessor::instance->processPGN(data, len, remoteIP, remotePort);
    }
}

void QNEthernetUDPHandler::handleRTCMPacket(const uint8_t* data, size_t len,
                                            const IPAddress& remoteIP, uint16_t remotePort) {
    // Process RTCM packet
    if (len > 0 && RTCMProcessor::instance) {
        RTCMProcessor::instance->processRTCM(data, len, remoteIP, remotePort);
    }
}

void QNEthernetUDPHandler::sendUDPPacket(uint8_t* data, int length) {
    // Check Ethernet link status
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "Cannot send UDP - no Ethernet link");
        return;
    }
    
    // Use the broadcast address from ConfigManager
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    IPAddress broadcastIP(destIP[0], destIP[1], destIP[2], destIP[3]);
    
    // Send packet
    udpSend.beginPacket(broadcastIP, configManager.getDestPort());
    udpSend.write(data, length);
    if (!udpSend.endPacket()) {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP packet");
    }
}

// Global function to replace sendUDPbytes
void sendUDPbytes(uint8_t* data, int length) {
    QNEthernetUDPHandler::sendUDPPacket(data, length);
}

// Send packet on port 9999 (for ESP32 bridge)
void QNEthernetUDPHandler::sendUDP9999Packet(uint8_t* data, int length) {
    // Check Ethernet link status
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "Cannot send UDP9999 - no Ethernet link");
        return;
    }
    
    // Use the broadcast address from ConfigManager
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    IPAddress broadcastIP(destIP[0], destIP[1], destIP[2], destIP[3]);
    
    // Send packet on port 9999
    udpSend.beginPacket(broadcastIP, 9999);
    udpSend.write(data, length);
    if (!udpSend.endPacket()) {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP9999 packet");
    }
}

void QNEthernetUDPHandler::enableDHCPServer(bool enable) {
    if (enable && !dhcpServerEnabled) {
        // Start DHCP server on port 67
        if (udpDHCP.begin(DHCP_SERVER_PORT)) {
            LOG_INFO(EventSource::NETWORK, "DHCP server started on port 67");
            LOG_INFO(EventSource::NETWORK, "DHCP range: 192.168.5.1 - 192.168.5.125");
        } else {
            LOG_ERROR(EventSource::NETWORK, "Failed to start DHCP server on port 67");
        }

        // Start multicast DNS responder socket on 224.0.0.251:5353 for .local lookups
        if (udpMDNS.beginMulticastWithReuse(kMDNSMulticastIP, kMDNSPort)) {
            LOG_INFO(EventSource::NETWORK, "=== mDNS RESPONDER STARTED ===");
            LOG_INFO(EventSource::NETWORK, "mDNS multicast responder listening on 224.0.0.251:5353");
            LOG_INFO(EventSource::NETWORK, "Will respond to queries for *.local names");
        } else {
            LOG_ERROR(EventSource::NETWORK, "!!! FAILED to start mDNS responder - multicast socket error !!!");
        }

        // Start DNS server on port 53
        if (udpDNS.begin(DNS_SERVER_PORT)) {
            LOG_INFO(EventSource::NETWORK, "DNS server started on port 53");
            LOG_INFO(EventSource::NETWORK, "DNS wildcard: *.local and *.aog queries reply with Teensy IP");
        } else {
            LOG_ERROR(EventSource::NETWORK, "Failed to start DNS server on port 53");
        }

        dhcpServerEnabled = true;
    } else if (!enable && dhcpServerEnabled) {
        // Stop DHCP and DNS servers
        udpDHCP.stop();
        udpDNS.stop();
        udpMDNS.stop();
        dhcpServerEnabled = false;
        LOG_INFO(EventSource::NETWORK, "DHCP/DNS/mDNS servers stopped");
    }
}

bool QNEthernetUDPHandler::isDHCPServerEnabled() {
    return dhcpServerEnabled;
}

void QNEthernetUDPHandler::handleDHCPPacket(const uint8_t* data, size_t len,
                                            const IPAddress& remoteIP, uint16_t remotePort) {
    if (len < sizeof(RIP_MSG)) {
        return;  // Packet too small
    }
    
    // Get our server IP
    IPAddress serverIP = Ethernet.localIP();
    byte serverIPBytes[4] = {serverIP[0], serverIP[1], serverIP[2], serverIP[3]};
    
    // Process DHCP request
    RIP_MSG* dhcpMsg = (RIP_MSG*)data;
    int replySize = DHCPreply(dhcpMsg, len, serverIPBytes, (char*)"aog");
    
    if (replySize > 0) {
        // Send DHCP reply to broadcast address on client port
        IPAddress broadcastIP(255, 255, 255, 255);
        udpDHCP.beginPacket(broadcastIP, DHCP_CLIENT_PORT);
        udpDHCP.write((uint8_t*)dhcpMsg, replySize);
        udpDHCP.endPacket();
        
        // Log DHCP activity
        static uint32_t lastDHCPLog = 0;
        if (millis() - lastDHCPLog > 1000) {  // Rate limit logging
            lastDHCPLog = millis();
            LOG_DEBUG(EventSource::NETWORK, "DHCP request processed from %d.%d.%d.%d",
                      remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
        }
    }
}

void QNEthernetUDPHandler::handleDNSPacket(const uint8_t* data, size_t len,
                                            const IPAddress& remoteIP, uint16_t remotePort) {
    if (len < sizeof(DNS_MSG)) {
        return;  // Packet too small
    }

    // Get our server IP
    IPAddress serverIP = Ethernet.localIP();
    byte serverIPBytes[4] = {serverIP[0], serverIP[1], serverIP[2], serverIP[3]};

    // Parse the queried hostname from the DNS question body.
    // DNS wire format: each label is <len><chars>, terminated by 0x00.
    // Pointer compression (0xC0) is not expected in simple queries.
    DNS_MSG* dnsMsg = (DNS_MSG*)data;
    const uint8_t* body = dnsMsg->BODY;
    int bodyMaxLen = (int)len - (int)sizeof(DNS_MSG);
    char queriedName[64] = "";
    {
        int pos = 0, opos = 0;
        bool first = true;
        while (pos < bodyMaxLen) {
            uint8_t segLen = body[pos++];
            if (segLen == 0) break;
            if ((segLen & 0xC0) == 0xC0) { pos++; break; } // pointer compression - skip
            if (!first && opos < (int)sizeof(queriedName) - 1) queriedName[opos++] = '.';
            first = false;
            for (int i = 0; i < segLen && pos < bodyMaxLen && opos < (int)sizeof(queriedName) - 1; i++) {
                queriedName[opos++] = (char)tolower(body[pos++]);
            }
        }
        queriedName[opos] = '\0';
    }

    // Wildcard *.local / *.aog handler: respond with the Teensy's own IP.
    // This enables DNS-based access for any module shortname (sc.local, wifi.local, etc.)
    // and for user-configured aliases configured on /dns-alias page.
    // The HTTP virtual-host router (SimpleHTTPServer) then routes to the right page.
    int nameLen = (int)strlen(queriedName);
    bool isLocalName = (nameLen >= 7 && strcasecmp(queriedName + nameLen - 6, ".local") == 0);
    bool isAogName = (nameLen >= 5 && strcasecmp(queriedName + nameLen - 4, ".aog") == 0);
    if (isLocalName || isAogName) {
        // Temporarily use the queried name as if it were in our name list – DNSreplyMulti
        // will then find it and reply with serverIP (no lease offset since lease=0).
        const char* tempList[] = { queriedName };
        int replySize = DNSreplyMulti(dnsMsg, (int)len, serverIPBytes, tempList, 1);
        if (replySize > 0) {
            udpDNS.beginPacket(remoteIP, remotePort);
            udpDNS.write((uint8_t*)dnsMsg, replySize);
            udpDNS.endPacket();
        }
        return;
    }

    // Non-*.local / non-*.aog queries: fall through to normal DNSreplyMulti (handles DHCP leases
    // and the static names for the Teensy's own interface).
    static const char *dnsNames[] = {"aio.local", "wifi.local", "gps.local", "steer.local", "aio.aog", "wifi.aog", "gps.aog", "steer.aog"};
    static const int dnsNameCount = 8;
    int replySize = DNSreplyMulti(dnsMsg, (int)len, serverIPBytes, dnsNames, dnsNameCount);

    if (replySize > 0) {
        // Send DNS reply back to the querying client
        udpDNS.beginPacket(remoteIP, remotePort);
        udpDNS.write((uint8_t*)dnsMsg, replySize);
        udpDNS.endPacket();
    }
}

void QNEthernetUDPHandler::handleMDNSPacket(const uint8_t* data, size_t len,
                                           const IPAddress& remoteIP, uint16_t remotePort) {
    if (!data || len < 12) {
        return;
    }

    // Only handle queries.
    if ((data[2] & 0x80) != 0) {
        return;
    }

    uint16_t questionCount = (uint16_t)((data[4] << 8) | data[5]);
    if (questionCount == 0) {
        return;
    }

    char queriedName[96];
    size_t questionEnd = 0;
    if (!decodeDnsName(data, len, 12, queriedName, sizeof(queriedName), &questionEnd)) {
        return;
    }

    if (questionEnd + 4 > len) {
        return;
    }

    uint16_t queryType = (uint16_t)((data[questionEnd] << 8) | data[questionEnd + 1]);
    uint16_t queryClass = (uint16_t)((data[questionEnd + 2] << 8) | data[questionEnd + 3]);
    bool preferUnicastReply = (queryClass & 0x8000u) != 0;

    size_t nameLen = strlen(queriedName);
    if (nameLen < 7 || strcasecmp(queriedName + nameLen - 6, ".local") != 0) {
        return;
    }

    // Answer A and ANY questions. Browsers normally also ask AAAA, but A is the
    // relevant record here because the AiO LAN is IPv4-only.
    if (queryType != 1 && queryType != 255) {
        return;
    }

    static uint32_t lastMDNSLog = 0;
    if (millis() - lastMDNSLog > 1000) {
        lastMDNSLog = millis();
        LOG_INFO(EventSource::NETWORK,
                 "mDNS query for %s from %d.%d.%d.%d (%s)",
                 queriedName,
                 remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3],
                 preferUnicastReply ? "unicast reply" : "multicast reply");
    }

    uint8_t response[512];
    if (questionEnd + 4 > sizeof(response)) {
        return;
    }

    memset(response, 0, sizeof(response));

    // Copy original header and question.
    memcpy(response, data, questionEnd + 4);

    // Standard authoritative mDNS answer with one question and one answer.
    response[2] = 0x84;
    response[3] = 0x00;
    response[4] = 0x00;
    response[5] = 0x01;
    response[6] = 0x00;
    response[7] = 0x01;
    response[8] = 0x00;
    response[9] = 0x00;
    response[10] = 0x00;
    response[11] = 0x00;

    size_t outPos = questionEnd + 4;
    if (outPos + 16 > sizeof(response)) {
        return;
    }

    IPAddress localIP = Ethernet.localIP();

    // Answer name pointer -> first question name at offset 12.
    response[outPos++] = 0xC0;
    response[outPos++] = 0x0C;
    // TYPE A
    response[outPos++] = 0x00;
    response[outPos++] = 0x01;
    // CLASS IN with cache-flush bit
    response[outPos++] = 0x80;
    response[outPos++] = 0x01;
    // TTL = 120 seconds
    response[outPos++] = 0x00;
    response[outPos++] = 0x00;
    response[outPos++] = 0x00;
    response[outPos++] = 0x78;
    // RDLENGTH = 4
    response[outPos++] = 0x00;
    response[outPos++] = 0x04;
    response[outPos++] = localIP[0];
    response[outPos++] = localIP[1];
    response[outPos++] = localIP[2];
    response[outPos++] = localIP[3];

    if (preferUnicastReply) {
        udpMDNS.send(remoteIP, kMDNSPort, response, outPos);
    } else {
        udpMDNS.send(kMDNSMulticastIP, kMDNSPort, response, outPos);
    }
}