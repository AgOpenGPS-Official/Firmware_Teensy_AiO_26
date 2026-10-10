// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// Watchdog.h
// Hardware watchdog (i.MX RT1062 WDOG1). If loop() stops feeding it, the MCU resets and
// all pins fall back to their reset state, so motor and valve outputs are released.

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>

namespace Watchdog {

// WDOG1 WCR layout: WT[15:8] (timeout = (WT+1) * 0.5 s), WDA(5) and SRS(4) active-low
// "do not assert" bits, WDE(2) enable. WDE cannot be cleared once set, only reset.
constexpr uint16_t WCR_WDE = 1u << 2;
constexpr uint16_t WCR_SRS = 1u << 4;
constexpr uint16_t WCR_WDA = 1u << 5;

inline void feed() {
    WDOG1_WSR = 0x5555;
    WDOG1_WSR = 0xAAAA;
}

// Timeout in half-second steps minus one: 1 = 1.0 s
inline void begin(uint8_t wt = 1) {
    WDOG1_WCR = (uint16_t)(((uint16_t)wt << 8) | WCR_WDE | WCR_SRS | WCR_WDA);
    feed();
}

// Longest timeout (128 s), for flash erase/write where nothing can feed it
inline void setMaxTimeout() {
    WDOG1_WCR = (uint16_t)((0xFFu << 8) | WCR_WDE | WCR_SRS | WCR_WDA);
    feed();
}

}  // namespace Watchdog

#endif // WATCHDOG_H
