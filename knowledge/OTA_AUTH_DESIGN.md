# OTA Authentication Design (issue #48)

Status: tier 1 implemented on branch `feat/48-ota-pin` (stacked on PR #51, which adds the steering interlock + motor stop before flash). Serial-only PIN provisioning was chosen.

## Starting point (verified in code)

- `/ota` page and `POST /api/ota/upload` are open to anyone who can reach the module's IP.
- Transport is plain HTTP on the LAN. No TLS is realistic on the Teensy.
- `SimpleHTTPServer::parseRequest` discards every request header (the "Skip remaining headers" loop), so nothing can read an `Authorization` header today.
- No credential store exists. Free EEPROM block: `MISC_CONFIG_ADDR` 1200-1299 (check actual use before claiming a slot). Changing the layout requires an `EEPROM_VERSION` bump (currently 112).
- `SimpleOTAHandler` writes Intel HEX to a buffer, then `applyUpdate()` calls `flash_move()`. There is no integrity or authenticity check beyond HEX checksums.

## Threat model

In scope: anyone else on the tractor/office/Wi-Fi-bridge LAN (a phone, a guest, malware on a laptop), and accidental uploads (wrong tab, wrong module, scripted scan).
Out of scope: an attacker who can sniff or modify traffic on the segment. Without TLS or signed images they can always win; the design says so openly rather than pretending otherwise.

## Options considered

| Option | Stops | Cost | Verdict |
|---|---|---|---|
| A. Shared PIN in a request header | casual/accidental access, scans | small | **Recommended (tier 1)** |
| B. Physical "arm" (hold steer button / serial command opens a 2 min update window) | all remote access, no secret on the wire | needs a physical action in the cab | good complement, optional |
| C. Challenge-response (HMAC of a nonce) | passive sniffing of the PIN | needs SHA-256/HMAC, nonce state, more JS | possible tier 1.5, not worth it first |
| D. Signed firmware (Ed25519, public key in firmware, verify before `flash_move`) | malicious or corrupt images even from a sniffer | key management, build/release tooling, verify code + flash cost | **Right long-term fix (tier 2)**, separate issue |
| E. TLS | everything on the wire | not feasible on this stack | rejected |

## Recommended design: tier 1, OTA PIN

1. **Credential**: one OTA PIN, 6-16 printable characters, stored in EEPROM (new field at `OTA_PIN_ADDR` 1216-1232, NUL-padded; erased EEPROM reads as "no PIN", so no `EEPROM_VERSION` bump and no reset of existing users' settings). Never returned by any API, never logged, never put in a URL or query string.
2. **Secure by default**: if no PIN is set, `POST /api/ota/upload` returns 403 and the OTA page shows "Set an OTA PIN first". There is no default PIN, so no shared factory secret.
3. **Presenting the PIN**: header `X-OTA-PIN`. The browser upload already uses `XMLHttpRequest`, so `setRequestHeader` is a one-line change on the page. `parseRequest` is changed to keep exactly this one header (bounded buffer, case-insensitive match) while still discarding the rest.
4. **Check**: length-independent constant-time compare. Checked after request line + headers, before any body byte is read, so a rejected request never reaches `SimpleOTAHandler`.
5. **Brute-force limiting** (RAM only): after 3 failures lock OTA for 60 s, doubling per further failure up to 15 min; reset on success or reboot. Return 429 while locked. Log each failure with the source IP at WARNING.
6. **Setting / changing the PIN** (decision needed, see below). Recommended: serial menu command (physical USB access, authenticated by being plugged in), plus changing it over the web only when the current PIN is supplied.
7. **Keep existing interlocks**: steering engaged returns 409 and the motor is stopped before flashing (PR #51). Authentication does not replace them.
8. **UI**: PIN field on the OTA page, sent as the header, held only in a JS variable (not `localStorage`). Clear error text for 403 / 429.

## Tier 1.5 (optional): physical arm

Add a short "update window": a serial command or the existing steer switch/button held for ~3 s sets `otaArmedUntil = millis() + 120000`. The upload endpoint additionally requires the window to be open. This removes the remote-attacker case completely (nothing on the wire is replayable) but requires someone in the cab. Can ship after tier 1 without changing it.

## Tier 2: signed firmware

Verify an Ed25519 signature over the HEX payload before `applyUpdate()` calls `flash_move()`; public key compiled in, private key held by the release process. This is the only option that defends against a hostile image from someone who can sniff the PIN. It needs a release-signing step in the build (`copy_hex.py`), a verify routine, and a policy for unsigned dev builds. Track as its own issue.

## Honest limits of tier 1

- The PIN crosses the LAN in clear text. Anyone who can sniff traffic can read it and replay it. Tier 1 raises the bar from "any device that finds the IP" to "someone with the PIN or a packet capture"; it is not a substitute for tier 2.
- `/ota` is not the only unauthenticated write path. `/api/restart`, `/api/device/settings`, `/api/can/config*`, `/api/um98x/write`, `/api/network/config` can all reconfigure or reset the module. Phase 2 should put the same PIN check in one place in the HTTP dispatch for all `POST`s, which is why step 3 should be implemented as a general "read the auth header" facility, not an OTA special case.

## Implementation plan

1. `SimpleHTTPServer`: capture `X-OTA-PIN` into a small bounded field in the request context.
2. `ConfigManager` + `EEPROMLayout.h`: OTA PIN field, getters/setters, version bump + defaults (empty = disabled).
3. New small `WebAuth` helper: constant-time compare, failure counter and lockout, one `checkOtaPin(client)`.
4. `SimpleWebManager::handleOTAUpload`: call it first (403 / 429).
5. `CommandHandler`: serial command to set/clear the PIN.
6. `TouchFriendlyOTAPage.h`: PIN field, header, error messages.
7. Tests/verification: build; manual checks for no PIN (403), wrong PIN (403 then 429 after 3), correct PIN (upload proceeds), PIN absent from logs/API responses.

## Decisions

1. **Provisioning**: serial only (menu key `O`: enter PIN twice, masked; an empty line clears it). No web provisioning.
2. **Scope**: OTA only in this change; gating all state-changing POSTs stays phase 2.
3. **Physical arm** (tier 1.5): not implemented, still optional.
4. **Signed firmware** (tier 2): still to be tracked as a separate issue.
